#!/usr/bin/env bash
# Runs only the state-machine model on a host, without any OEM inputs.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" \
 "$ROOT/tests/test_handoff_lifecycle.c" \
 "$ROOT/project/source/src/cp_session_handoff.c" \
 -o "$ROOT/cloud12_result/handoff_model_test"
"$ROOT/cloud12_result/handoff_model_test" | tee "$ROOT/cloud12_result/handoff_model.log"
