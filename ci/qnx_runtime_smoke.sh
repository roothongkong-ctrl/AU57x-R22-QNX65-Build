#!/usr/bin/env bash
# Runs on a GitHub-hosted runner, with the locally built QNX GCC Docker image.
# Builds a QNX native smoke executable, boots QNX in QEMU and dlopens our .so.
# Research-only: no OEM binaries, no MFi, no vehicle/network interaction.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/qnx_build_result"
UP=/tmp/qnx65-toolchain
LIB="$OUT/libau57x_r22_native_qnx_REVIEW_ONLY.so"
BIN="$OUT/au57x_qnx_dlopen_smoke"
mkdir -p "$OUT/logs"
test -s "$LIB"
test -f "$UP/tests/qemu-run.sh"
command -v qemu-system-arm
docker tag au57x-qnx65-cc:8.5 qnx65-armv7-toolchain:8.5
docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src \
  au57x-qnx65-cc:8.5 sh -c \
  'arm-unknown-nto-qnx6.5.0eabi-gcc -std=c99 -O2 -Wall -Wextra -Werror \
  -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp \
  -o qnx_build_result/au57x_qnx_dlopen_smoke ci/qnx_dlopen_smoke.c'
file "$BIN" | tee "$OUT/logs/qnx_smoke_file.txt"
# Extend upstream's single-program IFS script with a single extra .so.
# The cloned toolchain is temporary; no QNX proprietary content is committed.
python3 - "$UP/tests/qemu-run.sh" "$UP/tests/qemu-run-au57x.sh" <<'PY'
from pathlib import Path
import sys
src=Path(sys.argv[1]).read_text()
a='cp "$BIN" "$WORK/$NAME"\n'
b='cp "$BIN" "$WORK/$NAME"\nif [ -n "${QNX_TEST_EXTRA_SO:-}" ]; then cp "$QNX_TEST_EXTRA_SO" "$WORK/"; fi\n'
assert src.count(a)==1, 'QEMU runner copy anchor has changed'
src=src.replace(a,b,1)
a='EOF\n\ndocker run --rm --platform=linux/amd64 -v "$WORK":/w'
b='EOF\nif [ -n "${QNX_TEST_EXTRA_SO:-}" ]; then printf "%s\\n" "$(basename "$QNX_TEST_EXTRA_SO")" >> "$WORK/test.build"; fi\n\ndocker run --rm --platform=linux/amd64 -v "$WORK":/w'
assert src.count(a)==1, 'QEMU runner IFS anchor has changed'
src=src.replace(a,b,1)
Path(sys.argv[2]).write_text(src)
PY
chmod +x "$UP/tests/qemu-run-au57x.sh"
QNX_TEST_EXTRA_SO="$LIB" QEMU_RUN_SECS=20 QEMU_TIMEOUT=80 \
 QEMU_KEEP_LOG="$OUT/logs/qnx_boot_console.log" \
 "$UP/tests/qemu-run-au57x.sh" "$BIN" 8.5 \
  2>&1 | tee "$OUT/logs/qnx_runtime_smoke.log"
grep -Fq AU57X_RUNTIME_DLOPEN_OK "$OUT/logs/qnx_runtime_smoke.log"
echo 'QNX 6.5 QEMU dynamic loader smoke PASS (no OEM calls)'
