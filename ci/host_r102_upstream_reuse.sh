#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
UPSTREAM=third_party/luka-dev/mib2q-carplay-rgi/wireless/graft/eir.c
EXPECTED=99f42933e7c6b98af11fb387a1e68727ffe31997
[ "$(git hash-object "$UPSTREAM")" = "$EXPECTED" ] || { echo "Upstream source mismatch" >&2; exit 1; }
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
    project/source/legacy_mu1005/bluetooth/au102_eir_advertisement.c \
    "$UPSTREAM" tests/test_r102_upstream_eir.c -o "$TMP/test_r102_eir"
"$TMP/test_r102_eir"
echo R102_UPSTREAM_SOURCE_SHA_VERIFIED
