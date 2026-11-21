#!/usr/bin/env bash
# Deterministic CSM parser/encoder memory-safety checks (host only).
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" \
 "$ROOT/tests/test_csm_parser_fuzz.c" \
 "$ROOT/project/source/src/cp_event.c" \
 "$ROOT/project/source/src/cp_wireless.c" \
 "$ROOT/project/source/src/cp_csm.c" \
 -o "$ROOT/cloud12_result/csm_parser_fuzz"
"$ROOT/cloud12_result/csm_parser_fuzz" | tee "$ROOT/cloud12_result/csm_parser_fuzz.log"
