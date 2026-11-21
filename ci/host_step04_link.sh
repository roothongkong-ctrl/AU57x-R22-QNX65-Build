#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" -I"$ROOT/project/source/legacy_mu1005/bluetooth" \
 "$ROOT/project/source/src/cp_step04_link.c" \
 "$ROOT/project/source/legacy_mu1005/bluetooth/mu1005_rf_ingress.c" \
 "$ROOT/tests/test_step04_link.c" \
 -o "$ROOT/cloud12_result/step04_link_test"
"$ROOT/cloud12_result/step04_link_test" | tee "$ROOT/cloud12_result/step04_link.log"
