#!/usr/bin/env bash
# Mock symbols live only inside this HOST process; never install on car.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -rdynamic \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" \
 "$ROOT/project/source/src/cp_step04_stock_mfi.c" \
 "$ROOT/project/source/src/cp_step04_iap2_auth.c" \
 "$ROOT/project/source/src/cp_oem_boundary.c" \
 "$ROOT/project/source/src/cp_wireless.c" \
 "$ROOT/project/source/src/cp_event.c" \
 "$ROOT/project/source/src/cp_csm.c" \
 "$ROOT/tests/test_step04_stock_mfi.c" \
 -ldl -o "$ROOT/cloud12_result/step04_stock_mfi_host"
"$ROOT/cloud12_result/step04_stock_mfi_host" | tee "$ROOT/cloud12_result/step04_stock_mfi.log"
