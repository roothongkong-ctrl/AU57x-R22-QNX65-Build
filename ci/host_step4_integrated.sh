#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/project/source/src"
BT="$ROOT/project/source/legacy_mu1005/bluetooth"
OUT="${TMPDIR:-/tmp}/au57x_r90_mfi_$$"
trap 'rm -f "$OUT"' EXIT
cc -std=c99 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer \
 -DAU57X_R98_HOST_TEST=1 -I"$SRC" -I"$BT" -I"$ROOT/project/source/legacy_mu1005/airplay" \
 "$ROOT/tests/test_step4_integrated.c" \
 "$SRC/cp_wcp_r45_remote.c" "$SRC/cp_wcp_autoconnect.c" \
 "$SRC/cp_step04_socket_io.c" "$SRC/cp_step04_tcp_bridge.c" \
 "$SRC/cp_step04_session.c" "$SRC/cp_step04_link.c" \
 "$SRC/cp_step04_iap2_auth.c" "$SRC/cp_step04_stock_mfi.c" \
 "$SRC/cp_wireless.c" "$SRC/cp_oem_boundary.c" \
 "$SRC/cp_csm.c" "$SRC/cp_event.c" \
 "$BT/mu1005_rf_ingress.c" "$BT/bt_iap2_owner_handoff.c" "$BT/rf_arg_experiment.c" \
 "$SRC/cp_cloud_pipeline.c" "$SRC/cp_radio_policy.c" "$SRC/cp_session_handoff.c" "$SRC/cp_wcp_handoff.c" \
 "$ROOT/project/source/legacy_mu1005/airplay/"{mu1005_r34_ap_server,native_exports,cprx_service,semantic}.c -rdynamic -ldl -o "$OUT"
"$OUT"
