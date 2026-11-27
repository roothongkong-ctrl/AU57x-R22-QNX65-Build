#!/usr/bin/env bash
set -euo pipefail
export ROOT="$PWD"
base="$RUNNER_TEMP/slogf-trial"
mkdir -p "$base"
sdk_image=qnx65-armv7-toolchain:8.5
gcc=arm-unknown-nto-qnx6.5.0eabi-gcc
# Build the actual SDK archive implementation, not a no-op or fake logging function.
sdk_ok=0
if docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src "$sdk_image" "$gcc" -shared -Wl,-u,slogf -Wl,--no-undefined -Wl,-soname,liblab-slogf.so /opt/qnx650/target/qnx6/armle-v7/lib/libc.a -lc -o /src/qnx-runtime-evidence/liblab-slogf.so >"$base/sdk-build.log" 2>&1; then
  sdk_ok=1
fi
echo "SDK_REAL_SLOGF_SHARED_BUILD=$sdk_ok"
for program in smartphone_integrator btstack; do
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
  cp lab/mu1005_slogf_trial/launch.c "$out/launch.c"
  cp "$private/profile.h" "$out/profile.h"
  docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src "$sdk_image" "$gcc" -std=gnu99 -D_QNX_SOURCE -Wall -Wextra -Werror -O2 "-DLAB_PRELOAD=\"/proc/boot/$lib\"" "qnx-runtime-evidence/$lane/launch.c" -lc -o "qnx-runtime-evidence/$lane/mu1005-oem-launch"
  cp toolchain/tests/qemu-run.sh "$runner"
  guest=layout
  if [ "$program" = btstack ];then guest=enable;fi
  python3 lab/mu1005_oem_launch/prepare_ifs.py "$runner" "$private" "$guest"
  QNX_OEM_PRIVATE="$private" QEMU_KEEP_LOG="$private-serial.log" QEMU_TIMEOUT=120 QEMU_RUN_SECS=75 sh "$runner" "$out/mu1005-oem-launch" 8.5 >"$private-runtime.log" 2>&1 || rc=$?
  python3 lab/mu1005_runtime_diagnosis/classify.py "$out/linker.json" "$private-runtime.log" "$private-serial.log"
  printf 'WRAPPER_EXIT=%s\n' "$rc" >"$out/trial-result.txt"
  tr -d '\r' <"$private-runtime.log" | grep -E '^(ORIGINAL_|SDIO_)' >>"$out/trial-result.txt" || true
  grep -F ORIGINAL_GUEST_BYTES_OK "$out/trial-result.txt"
  grep -F ORIGINAL_LAUNCH_DIAGNOSTIC_COMPLETE_NOT_CONNECTION "$out/trial-result.txt"
}
pids=(); lanes=()
for program in smartphone_integrator btstack; do
 for mode in oem sdk; do
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
done
exit "$failed"
