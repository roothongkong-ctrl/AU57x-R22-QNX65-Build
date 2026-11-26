#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/au57x-x43-scn"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/project/source/legacy_mu1005/bluetooth" \
  "$ROOT/project/source/legacy_mu1005/bluetooth/inbound.c" \
  "$ROOT/tests/test_mu1005_rf_scn_extended.c" \
  -o "$TMP/test_mu1005_rf_scn_extended"
"$TMP/test_mu1005_rf_scn_extended"
