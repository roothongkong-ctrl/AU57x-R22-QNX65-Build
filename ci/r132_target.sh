#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
sha256sum -c R132_TRACE_SHA256.txt
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
RE=arm-unknown-nto-qnx6.5.0eabi-readelf
"$CC" -dM -E -x c /dev/null >build/qnx/r132_macros.txt
grep -q '__QNXNTO__' build/qnx/r132_macros.txt
grep -q '__arm__' build/qnx/r132_macros.txt
"$CC" -std=c11 -D_QNX_SOURCE=1 -include sys/types.h -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -Inative_rf native_rf/r131_trace_entry.c native_rf/r132_trace_bridge.c native_rf/r132_trace_thunk.S ci/r132_arm_trace_smoke.c -o build/qnx/r132_arm_trace_smoke
"$RE" -h -Ws build/qnx/r132_arm_trace_smoke >build/qnx/r132_readelf.txt
grep -Eq 'Machine:.*ARM' build/qnx/r132_readelf.txt
if grep -E 'UND.*(__atomic_|__sync_)' build/qnx/r132_readelf.txt; then exit 2; fi
sha256sum build/qnx/r132_arm_trace_smoke >build/qnx/R132_TARGET_SHA256.txt
