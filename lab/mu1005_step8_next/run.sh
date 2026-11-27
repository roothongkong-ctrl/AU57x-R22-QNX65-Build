#!/usr/bin/env bash
set -euo pipefail
export ROOT="$PWD"
base="$RUNNER_TEMP/step8-parser"
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

p="$base/btstack"
private="$p/original"
out="$ROOT/qnx-runtime-evidence/reader"
mkdir -p "$out"
cp "$p/staged.json" "$p/sdk.json" "$out/"
python3 lab/mu1005_slogf_trial/stage_provider.py "$RUNNER_TEMP/combined" "$private" sdk "$out/provider.json" "$ROOT/qnx-runtime-evidence/liblab-slogf.so"
test -s "$private/libutil.so"
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src "$sdk_image" "$gcc" -std=gnu99 -D_QNX_SOURCE -Wall -Wextra -Werror -O2 lab/mu1005_step8_next/parser_probe.c -lc -o "$out/step8-parser"
runner="$ROOT/toolchain/tests/qemu-step8.sh"
cp toolchain/tests/qemu-run.sh "$runner"
python3 lab/mu1005_oem_launch/prepare_ifs.py "$runner" "$private" baseline
QNX_OEM_PRIVATE="$private" QEMU_KEEP_LOG="$out/serial.log" QEMU_TIMEOUT=85 QEMU_RUN_SECS=35 sh "$runner" "$out/step8-parser" 8.5 | tee "$out/runtime.log"
python3 - "$private/connectivity.json" "$out/serial.log" <<'PY'
from pathlib import Path
import sys
b=Path(sys.argv[1]).read_bytes();h=2166136261
for x in b:h=((h^x)*16777619)&0xffffffff
s=Path(sys.argv[2]).read_text()
assert all("STEP8_NATIVE_EXPORT_OK "+str(i) in s for i in range(4))
assert f"STEP8_FIXTURE_READ size={len(b)} fnv={h:08x}" in s
assert "R105166_NATIVE_EXPORT_AND_FIXTURE_READ_NOT_PARSE" in s
print("R105166_QNX_NATIVE_EXPORT_PREFLIGHT_PASS")
PY
