#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
cc -std=gnu99 -Wall -Wextra -Werror -DHOST_TEST lab/mu1005_config_path/probe.c -o "$tmp/probe"
printf 'fixture\n' >"$tmp/original"
ln -s "$tmp/original" "$tmp/alias"
cp "$tmp/original" "$tmp/distinct"
"$tmp/probe" "$tmp/original" "$tmp/alias" | tee "$tmp/alias.log"
grep -Fx "SAME_FILE $tmp/original $tmp/alias" "$tmp/alias.log"
"$tmp/probe" "$tmp/original" "$tmp/distinct" | tee "$tmp/distinct.log"
grep -Fx "DISTINCT_FILE $tmp/original $tmp/distinct" "$tmp/distinct.log"
"$tmp/probe" "$tmp/original" "$tmp/missing" | tee "$tmp/missing.log"
grep -F "PATH_UNAVAILABLE $tmp/missing" "$tmp/missing.log"
! grep -E '^SAME_FILE|^DISTINCT_FILE' "$tmp/missing.log"
cmp "$tmp/original" "$tmp/distinct"
sh -n lab/mu1005_config_path/probe.sh
echo CONFIG_PATH_FIXTURE_PASS
