#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -D_DEFAULT_SOURCE -rdynamic \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" -I"$ROOT/project/source/legacy_mu1005/bluetooth" \
 "$ROOT/tests/test_step04_tcp_bridge.c" \
 "$ROOT/project/source/src/cp_step04_tcp_bridge.c" \
 "$ROOT/project/source/src/cp_step04_session.c" \
 "$ROOT/project/source/src/cp_step04_link.c" \
 "$ROOT/project/source/src/cp_step04_stock_mfi.c" \
 "$ROOT/project/source/src/cp_step04_iap2_auth.c" \
 "$ROOT/project/source/src/cp_oem_boundary.c" \
 "$ROOT/project/source/src/cp_wireless.c" \
 "$ROOT/project/source/src/cp_event.c" \
 "$ROOT/project/source/src/cp_csm.c" \
 "$ROOT/project/source/legacy_mu1005/bluetooth/mu1005_rf_ingress.c" \
 -ldl -o "$ROOT/cloud12_result/step04_tcp_host"
"$ROOT/cloud12_result/step04_tcp_host" | tee "$ROOT/cloud12_result/step04_tcp_host.log"
