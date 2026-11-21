#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" \
 "$ROOT/project/source/src/cp_radio_policy.c" \
 "$ROOT/tests/test_radio_policy.c" \
 -o "$ROOT/cloud12_result/radio_policy_host"
"$ROOT/cloud12_result/radio_policy_host" | tee "$ROOT/cloud12_result/radio_policy.log"
