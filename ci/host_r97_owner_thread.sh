#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc=${CC:-gcc}
"$cc" -std=c99 -O1 -g -Wall -Wextra -Werror -pthread \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Iproject/source/legacy_mu1005/bluetooth \
 project/source/legacy_mu1005/bluetooth/bt_thread_owner.c \
 tests/test_r97_bt_thread_owner.c -o "$work/r97_native_threads"
"$work/r97_native_threads"
echo 'R97 actual pthread trampoline/owner-origin test PASS; this does NOT prove original BlueSDK callback.'
