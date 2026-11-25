#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/host_results"; mkdir -p "$OUT"
CC="${CC:-gcc}"
"$CC" -std=c99 -O1 -Wall -Wextra -Werror -g -D_DEFAULT_SOURCE \
  -I"$ROOT/project/source/legacy_mu1005/airplay" \
  "$ROOT/tests/test_wifi_interface.c" \
  "$ROOT/project/source/legacy_mu1005/airplay/wifi_interface.c" \
  -o "$OUT/test_wifi_interface"
"$OUT/test_wifi_interface" | tee "$OUT/wifi_interface.log"
