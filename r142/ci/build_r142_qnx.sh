#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
CC=\${QNX_CC:-arm-unknown-nto-qnx6.5.0eabi-gcc}
RE=\${QNX_READELF:-arm-unknown-nto-qnx6.5.0eabi-readelf}
command -v "$CC" >/dev/null || { echo 'BLOCKED: QNX 6.5 ARM compiler unavailable'; exit 30; }
command -v "$RE" >/dev/null || { echo 'BLOCKED: QNX readelf unavailable'; exit 31; }
mkdir -p build/qnx
"$CC" -dM -E -x c /dev/null >build/qnx/r142_macros.txt
grep -q __QNXNTO__ build/qnx/r142_macros.txt
grep -q __arm__ build/qnx/r142_macros.txt
FLAGS=(-std=c11 -D_QNX_SOURCE=1 -DR142_REVIEW_ONLY=1 -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp)
"$CC" "\${FLAGS[@]}" -fPIC -shared -Wl,--hash-style=sysv -Wl,--no-undefined preload_review/r142_thread_name_probe.c -lc -o build/qnx/libau57x_r142_thread_probe_REVIEW.so
"$CC" "\${FLAGS[@]}" tests/r142_qnx_smoke.c -lc -o build/qnx/r142_qnx_smoke
"$RE" -h -d -Ws build/qnx/libau57x_r142_thread_probe_REVIEW.so >build/qnx/r142_elf.txt
grep -Eq 'Machine:.*ARM' build/qnx/r142_elf.txt
grep -Fq 'Shared library: [libc.so.3]' build/qnx/r142_elf.txt
grep -Eq 'FUNC[[:space:]]+GLOBAL.*pthread_setname_np$' build/qnx/r142_elf.txt
grep -Eq 'FUNC[[:space:]]+GLOBAL.*r142_thread_probe_status$' build/qnx/r142_elf.txt
sha256sum build/qnx/libau57x_r142_thread_probe_REVIEW.so build/qnx/r142_qnx_smoke >build/qnx/R142_TARGET_SHA256.txt
printf 'R142_QNX_TARGET_BUILD_PASS; QNX_RUNTIME_PENDING; OEM_EXECUTED=false; NO_INSTALL\n'
