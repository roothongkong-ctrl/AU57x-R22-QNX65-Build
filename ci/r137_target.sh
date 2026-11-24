#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
sha256sum -c R137_PRELOAD_SHA256.txt
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
RE=arm-unknown-nto-qnx6.5.0eabi-readelf
"$CC" -dM -E -x c /dev/null >build/qnx/r137_macros.txt
grep -q __QNXNTO__ build/qnx/r137_macros.txt
grep -q __arm__ build/qnx/r137_macros.txt
set -- -std=c11 -D_QNX_SOURCE=1 -include sys/types.h -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp
"$CC" "$@" -fPIC -shared -Wl,--hash-style=sysv -Wl,--no-undefined preload_review/preload_probe.c -Wl,--no-as-needed -lc -Wl,--as-needed -o build/qnx/libau57x_r137_preload_REVIEW.so
"$CC" "$@" preload_review/preload_test.c -o build/qnx/r137_preload_test
"$RE" -h -d -Ws build/qnx/libau57x_r137_preload_REVIEW.so >build/qnx/r137_preload_elf.txt
grep -Eq 'Machine:.*ARM' build/qnx/r137_preload_elf.txt
grep -Fq 'Shared library: [libc.so.3]' build/qnx/r137_preload_elf.txt
grep -Eq 'FUNC[[:space:]]+GLOBAL.*r137_preload_pid$' build/qnx/r137_preload_elf.txt
sha256sum build/qnx/libau57x_r137_preload_REVIEW.so build/qnx/r137_preload_test >build/qnx/R137_TARGET_SHA256.txt
