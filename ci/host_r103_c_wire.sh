#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cc -std=c11 -O2 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -pedantic \
 -Iproject/source/legacy_mu1005/bluetooth \
 project/source/legacy_mu1005/bluetooth/mu1005_rf_ingress.c \
 tests/test_r103_c_bridge_wire.c -o "$TMP/test_wire"
"$TMP/test_wire"
