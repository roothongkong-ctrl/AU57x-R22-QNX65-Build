#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/project/source/src"
BT="$ROOT/project/source/legacy_mu1005/bluetooth"
OUT="${TMPDIR:-/tmp}/au57x_r89_remote_$$"
trap 'rm -f "$OUT"' EXIT
cc -std=c99 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer \
 -I"$SRC" -I"$BT" \
 "$ROOT/tests/test_wcp_r45_remote.c" \
 "$SRC/cp_wcp_r45_remote.c" "$SRC/cp_wcp_autoconnect.c" \
 "$SRC/cp_step04_socket_io.c" "$SRC/cp_step04_tcp_bridge.c" \
 "$SRC/cp_step04_session.c" "$SRC/cp_step04_link.c" \
 "$SRC/cp_step04_iap2_auth.c" "$SRC/cp_step04_stock_mfi.c" \
 "$SRC/cp_wireless.c" "$SRC/cp_oem_boundary.c" \
 "$SRC/cp_csm.c" "$SRC/cp_event.c" \
 "$BT/mu1005_rf_ingress.c" -o "$OUT"
"$OUT"