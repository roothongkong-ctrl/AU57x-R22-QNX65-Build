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
# R132 fix: C-only forced include must never be passed to assembly.
"$CC" -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -c native_rf/r132_trace_thunk.S -o build/qnx/r132_trace_thunk.o
"$CC" "$@" -Inative_rf native_rf/r131_trace_entry.c native_rf/r132_trace_bridge.c ci/r132_arm_trace_smoke.c build/qnx/r132_trace_thunk.o -o build/qnx/r132_arm_trace_smoke
"$RE" -h -Ws build/qnx/r132_arm_trace_smoke >build/qnx/r132_readelf.txt
if grep -E 'UND.*(__atomic_|__sync_)' build/qnx/r132_readelf.txt; then exit 2; fi
