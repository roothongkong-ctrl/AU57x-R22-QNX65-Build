#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/host_results"; mkdir -p "$OUT"
SRC="$ROOT/project/source/legacy_mu1005/bluetooth"
CC="${CC:-gcc}"
"$CC" -std=c99 -O1 -Wall -Wextra -Werror -g -D_DEFAULT_SOURCE \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$SRC" "$ROOT/tests/test_bt_contract.c" \
 "$SRC/mu1005_rf_ingress.c" "$SRC/mu1005_ipc_stream.c" "$SRC/mu1005_socket_pump.c" \
 -o "$OUT/test_bt_contract_asan"
"$OUT/test_bt_contract_asan" | tee "$OUT/host_bt_contract.log"
