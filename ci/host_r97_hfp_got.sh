#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT
cc=${CC:-gcc}
"$cc" -std=c99 -g -O1 -Wall -Wextra -Werror -pthread \
 -DAU57X_R97_HOST_TEST=1 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Iproject/source/legacy_mu1005/bluetooth \
 project/source/legacy_mu1005/bluetooth/bt_thread_owner.c \
 project/source/legacy_mu1005/bluetooth/bt_got_observer.c \
 tests/test_r97_hfp_got_observer.c -o "$w/r97_hfp"
"$w/r97_hfp"
