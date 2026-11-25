#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
QNX_BIN="${QNX_HOST:-/opt/qnx650/host/linux/x86}/usr/bin"
PATH="$QNX_BIN:$PATH"
export PATH
CC=${QNX_CC:-$QNX_BIN/arm-unknown-nto-qnx6.5.0eabi-gcc}
RE=${QNX_READELF:-$QNX_BIN/arm-unknown-nto-qnx6.5.0eabi-readelf}
if [ ! -x "$CC" ]; then
  echo "BLOCKED: QNX compiler missing: $CC"
  echo "PATH=$PATH"
  ls -l "$QNX_BIN" | head -40 || true
  find "${QNX_HOST:-/opt/qnx650/host/linux/x86}" -maxdepth 5 -name '*qnx6.5.0eabi*gcc*' -print || true
  exit 30
fi
if [ ! -x "$RE" ]; then
  echo "BLOCKED: QNX readelf missing: $RE"
  ls -l "$QNX_BIN" | head -40 || true
  exit 31
fi
mkdir -p build/qnx
"$CC" -dM -E -x c /dev/null >build/qnx/r142_macros.txt
grep -q __QNXNTO__ build/qnx/r142_macros.txt
grep -q __arm__ build/qnx/r142_macros.txt
FLAGS=(-std=c11 -D_QNX_SOURCE=1 -DR142_REVIEW_ONLY=1 -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp)
"$CC" "${FLAGS[@]}" -fPIC -shared -Wl,--hash-style=sysv -Wl,--no-undefined preload_review/r142_thread_name_probe.c -lc -o build/qnx/libau57x_r142_thread_probe_REVIEW.so
"$CC" "${FLAGS[@]}" tests/r142_qnx_smoke.c -lc -o build/qnx/r142_qnx_smoke
"$RE" -h -d -Ws build/qnx/libau57x_r142_thread_probe_REVIEW.so >build/qnx/r142_elf.txt
grep -Eq 'Machine:.*ARM' build/qnx/r142_elf.txt
grep -Fq 'Shared library: [libc.so.3]' build/qnx/r142_elf.txt
grep -Eq 'FUNC[[:space:]]+GLOBAL.*pthread_setname_np$' build/qnx/r142_elf.txt
grep -Eq 'FUNC[[:space:]]+GLOBAL.*r142_thread_probe_status$' build/qnx/r142_elf.txt
sha256sum build/qnx/libau57x_r142_thread_probe_REVIEW.so build/qnx/r142_qnx_smoke >build/qnx/R142_TARGET_SHA256.txt
printf 'R142_QNX_TARGET_BUILD_PASS; QNX_RUNTIME_PENDING; OEM_EXECUTED=false; NO_INSTALL\n'
