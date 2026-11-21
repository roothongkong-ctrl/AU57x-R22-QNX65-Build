#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" \
 "$ROOT/project/source/src/cp_mu1005_callback.c" \
 "$ROOT/tests/test_mu1005_callback.c" \
 -o "$ROOT/cloud12_result/mu1005_callback_test"
"$ROOT/cloud12_result/mu1005_callback_test" | tee "$ROOT/cloud12_result/mu1005_callback.log"
