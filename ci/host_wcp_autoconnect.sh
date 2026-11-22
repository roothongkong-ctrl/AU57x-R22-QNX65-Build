#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" "$ROOT/tests/test_wcp_autoconnect.c" \
 "$ROOT/project/source/src/cp_wcp_autoconnect.c" \
 -o "$ROOT/cloud12_result/wcp_autoconnect_host"
"$ROOT/cloud12_result/wcp_autoconnect_host" | tee "$ROOT/cloud12_result/wcp_autoconnect.log"
