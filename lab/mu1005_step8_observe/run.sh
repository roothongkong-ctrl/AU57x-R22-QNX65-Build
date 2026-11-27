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
p="$base/connectivity_launcher"
mkdir -p "$p"
private="$p/original"
out="$ROOT/qnx-runtime-evidence/reader"
mkdir -p "$out"
python3 lab/mu1005_step8_observe/stage.py "$RUNNER_TEMP/combined" "$private" connectivity_launcher "$out/staged.json"
python3 lab/mu1005_step8_observe/stage_sdk.py toolchain "$private" "$out/sdk.json" "$out/staged.json" connectivity_launcher
python3 lab/mu1005_slogf_trial/stage_provider.py "$RUNNER_TEMP/combined" "$private" sdk "$out/provider.json" "$ROOT/qnx-runtime-evidence/liblab-slogf.so"
cp "$private/profile.h" "$out/profile.h"
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src "$sdk_image" "$gcc" -std=gnu99 -D_QNX_SOURCE -Wall -Wextra -Werror -O2 -shared -fPIC lab/mu1005_step8_observe/observe.c lab/mu1005_step8_observe/forward.S -lc -o /src/qnx-runtime-evidence/reader/libstep8observe.so
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src "$sdk_image" "$gcc" -std=gnu99 -D_QNX_SOURCE -Wall -Wextra -Werror -O2 -I /src/qnx-runtime-evidence/reader lab/mu1005_step8_observe/supervisor.c -lc -o /src/qnx-runtime-evidence/reader/step8-observe
cp "$out/libstep8observe.so" "$private/"
runner="$ROOT/toolchain/tests/qemu-step8-observe.sh"
cp toolchain/tests/qemu-run.sh "$runner"
python3 lab/mu1005_oem_launch/prepare_ifs.py "$runner" "$private" baseline
QNX_OEM_PRIVATE="$private" QEMU_KEEP_LOG="$out/serial.log" QEMU_TIMEOUT=85 QEMU_RUN_SECS=35 sh "$runner" "$out/step8-observe" 8.5 | tee "$out/runtime.log"
python3 - "$out/serial.log" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
assert "STEP8_STOCK_LAUNCHER_BYTES_OK" in s
hits=re.findall(r"STEP8_ORIGINAL_PARSE_ARGUMENT ([^\r\n]+)",s)
print("STEP8_STOCK_PARSER_OBSERVED_ARGUMENTS",repr(hits))
assert "STEP8_OBSERVER_FORWARD_UNAVAILABLE" not in s
assert any(x.rsplit("/",1)[-1]=="connectivity.json" for x in hits),"NO_STOCK_TARGET_PARSE_CALL_OBSERVED"
print("R105169_STOCK_TARGET_PARSE_CALL_OBSERVED_QNX_ONLY")
PY
