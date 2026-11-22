#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/project/source/src"
BT="$ROOT/project/source/legacy_mu1005/bluetooth"
OUT="${TMPDIR:-/tmp}/au57x_r88_r45_$$"
trap 'rm -f "$OUT"' EXIT
cc -std=c99 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer \
  -I"$SRC" -I"$BT" \
  "$ROOT/tests/test_wcp_r45_ingress.c" \
  "$SRC/cp_wcp_r45_ingress.c" "$SRC/cp_wcp_autoconnect.c" \
  "$BT/mu1005_rf_ingress.c" -o "$OUT"
"$OUT"
