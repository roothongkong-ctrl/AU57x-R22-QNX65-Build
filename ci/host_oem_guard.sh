#!/usr/bin/env bash
# Host-only no-radio proof gate.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" \
 "$ROOT/tests/test_oem_csm_guard.c" \
 "$ROOT/project/source/src/cp_oem_boundary.c" \
 "$ROOT/project/source/src/cp_wireless.c" \
 "$ROOT/project/source/src/cp_event.c" \
 "$ROOT/project/source/src/cp_csm.c" \
 -o "$ROOT/cloud12_result/oem_csm_guard_test"
"$ROOT/cloud12_result/oem_csm_guard_test" | tee "$ROOT/cloud12_result/oem_csm_guard.log"
