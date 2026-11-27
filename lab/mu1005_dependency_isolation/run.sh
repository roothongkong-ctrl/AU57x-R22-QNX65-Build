#!/usr/bin/env bash
set -euo pipefail
export ROOT="$PWD"
base="$RUNNER_TEMP/slogf-trial"
mkdir -p "$base"
sdk_image=qnx65-armv7-toolchain:8.5
gcc=arm-unknown-nto-qnx6.5.0eabi-gcc
# Exact alternative archive identified in R105.145; never use libc.a slog.o.
program=$1
case "$program" in btstack|bluetooth|smartphone_integrator);;*)exit 2;;esac
mkdir -p qnx-runtime-evidence
docker run --rm -i --platform linux/amd64 --entrypoint /bin/bash -v "$PWD:/src" "$sdk_image" -s >qnx-runtime-evidence/sdk-build.log 2>&1 <<'SDK'
set -euxo pipefail
cc=arm-unknown-nto-qnx6.5.0eabi-gcc
nm=arm-unknown-nto-qnx6.5.0eabi-nm
ar=arm-unknown-nto-qnx6.5.0eabi-ar
re=arm-unknown-nto-qnx6.5.0eabi-readelf
f=/opt/qnx650/target/qnx6/armle-v7/lib/libcS.a
test -s "$f"
"$nm" -A --defined-only "$f" | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/' >/tmp/defs
test "$(wc -l </tmp/defs)" -eq 1
member=$(awk -F: '{print $(NF-1)}' /tmp/defs)
"$ar" p "$f" "$member" >/tmp/slog-pic.o
test -s /tmp/slog-pic.o
"$re" -rW /tmp/slog-pic.o >/tmp/relocs
if grep -E 'R_ARM_MOV[WT]_ABS' /tmp/relocs;then exit 3;fi
"$cc" -shared -Wl,--no-undefined -Wl,-soname,liblab-slogf.so /tmp/slog-pic.o -lc -o /src/qnx-runtime-evidence/liblab-slogf.so
"$nm" -D --defined-only /src/qnx-runtime-evidence/liblab-slogf.so | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {f=1} END{exit !f}'
sha256sum "$f" /tmp/slog-pic.o /src/qnx-runtime-evidence/liblab-slogf.so
echo SDK_CONFIRMED_PIC_LINK_OK
SDK
grep -Fx SDK_CONFIRMED_PIC_LINK_OK qnx-runtime-evidence/sdk-build.log
sdk_ok=1
for program in "$program"; do
  p="$base/$program"
  mkdir -p "$p"
  python3 lab/mu1005_oem_launch/stage.py "$RUNNER_TEMP/combined" "$p/original" "$program" "$p/staged.json"
  if [ "$program" = smartphone_integrator ]; then
    python3 lab/mu1005_runtime_diagnosis/stage_si_sdk.py toolchain "$p/original" "$p/sdk.json" "$p/staged.json"
  else
    python3 lab/mu1005_bt_sdk/stage_sdk.py toolchain "$p/original" "$p/sdk.json" "$p/staged.json" "$program"
  fi
done
trial(){
  set -euo pipefail
  local program="$1" mode="$2" p="$base/$1" lane="$1-$2" lib guest rc=0
  local out="$ROOT/qnx-runtime-evidence/$lane" private="$base/$lane" runner="$ROOT/toolchain/tests/qemu-$lane.sh"
  mkdir -p "$out"
  cp "$p/staged.json" "$p/sdk.json" "$out/"
  if [ "$mode" = sdk ] && [ "$sdk_ok" != 1 ]; then echo SDK_REAL_SLOGF_BUILD_BLOCKED >"$out/trial-result.txt";return 1;fi
  cp -a "$p/original" "$private"
  python3 lab/mu1005_slogf_trial/stage_provider.py "$RUNNER_TEMP/combined" "$private" "$mode" "$out/provider.json" "$ROOT/qnx-runtime-evidence/liblab-slogf.so"
  if [ "$mode" = sdk ]; then lib=liblab-slogf.so;else lib=liblogging.so.1;fi
  cp lab/mu1005_dependency_isolation/launch.c "$out/launch.c"
  python3 lab/mu1005_dependency_isolation/profile.py "$private/$program" "$out/profile.h" "$out/dependencies.json"
  docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src "$sdk_image" "$gcc" -std=gnu99 -D_QNX_SOURCE -Wall -Wextra -Werror -O2 "-DLAB_PRELOAD=\"/proc/boot/$lib\"" "qnx-runtime-evidence/$lane/launch.c" -lc -o "qnx-runtime-evidence/$lane/mu1005-oem-launch"
  cp toolchain/tests/qemu-run.sh "$runner"
  guest=layout
  if [ "$program" != smartphone_integrator ];then guest=enable;fi
  python3 lab/mu1005_oem_launch/prepare_ifs.py "$runner" "$private" "$guest"
  QNX_OEM_PRIVATE="$private" QEMU_KEEP_LOG="$private-serial.log" QEMU_TIMEOUT=180 QEMU_RUN_SECS=150 sh "$runner" "$out/mu1005-oem-launch" 8.5 >"$private-runtime.log" 2>&1 || rc=$?
  python3 lab/mu1005_runtime_diagnosis/classify.py "$out/linker.json" "$private-runtime.log" "$private-serial.log"
  printf 'WRAPPER_EXIT=%s\n' "$rc" >"$out/trial-result.txt"
  tr -d '\r' <"$private-runtime.log" | grep -E '^(DEP_|PIC_PROVIDER_)' >>"$out/trial-result.txt" || true
  python3 lab/mu1005_dependency_isolation/check.py "$out/dependencies.json" "$out/trial-result.txt" "$out/isolated-result.json"
  cat "$out/isolated-result.json"
}
pids=(); lanes=()
for program in "$program"; do
 for mode in sdk; do
   lane="$program-$mode"
   (trial "$program" "$mode") >"$base/$lane-host.log" 2>&1 &
   pids+=("$!"); lanes+=("$lane")
 done
done
failed=0
for i in "${!pids[@]}"; do
  lane="${lanes[i]}"
  if wait "${pids[i]}"; then echo "SLOGF_TRIAL_DIAGNOSTIC_DONE $lane";else echo "SLOGF_TRIAL_BLOCKED $lane";failed=1;fi
  out="$ROOT/qnx-runtime-evidence/$lane"
  if [ -f "$out/trial-result.txt" ];then cat "$out/trial-result.txt";fi
  if [ -f "$out/linker.json" ];then cat "$out/linker.json";fi
  if [ ! -f "$out/trial-result.txt" ];then echo "NO_GUEST_RESULT $lane";fi
done
exit "$failed"
