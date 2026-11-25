#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
cc -std=gnu99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie ap_live.c qnx_process.c cp_csm.c test.c -o "$t/test"
"$t/test"
