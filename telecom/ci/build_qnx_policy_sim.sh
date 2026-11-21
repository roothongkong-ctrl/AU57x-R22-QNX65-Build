#!/usr/bin/env bash
# QNX6.5 ARMv7 cross-build: pure C policy simulator, NOT an OEM plugin.
set -Eeuo pipefail
ROOT=/src
OUT="$ROOT/telecom_qnx_result"
mkdir -p "$OUT"
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
command -v "$CC"
"$CC" --version | head -1
test -r "$QNX_TARGET/armle-v7/lib/libc.so.3" \
  || { echo 'QNX65_TARGET_LIBC_MISSING'; exit 42; }
"$CC" -std=c99 -O2 -Wall -Wextra -Werror -fPIC \
  -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp \
  -mno-unaligned-access -c \
  "$ROOT/telecom/qnx/ct_network_policy_sim.c" \
  -o "$OUT/ct_network_policy_sim.o"
"$CC" -shared -Wl,--hash-style=sysv \
  -o "$OUT/libau57x_ct_r04_HYPOTHESIS_REVIEW_ONLY.so" \
  "$OUT/ct_network_policy_sim.o" \
  -Wl,--no-as-needed -lc -Wl,--as-needed
readelf -h "$OUT/libau57x_ct_r04_HYPOTHESIS_REVIEW_ONLY.so" | tee "$OUT/header.txt"
readelf -d "$OUT/libau57x_ct_r04_HYPOTHESIS_REVIEW_ONLY.so" | tee "$OUT/deps.txt"
file "$OUT/libau57x_ct_r04_HYPOTHESIS_REVIEW_ONLY.so" | tee "$OUT/type.txt"
echo 'QNX_RESEARCH_LIB_CREATED=YES'
echo 'OEM_HOOK_WIRED=NO'
echo 'INSTALLABLE=NO'
echo 'TELECOM_LTE_REGISTRATION_VERIFIED=NO'
