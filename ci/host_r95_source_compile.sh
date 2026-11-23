#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
inc=(-Iproject/source/src -Iproject/source/legacy_mu1005/bluetooth -Iproject/source/legacy_mu1005/airplay)
count=0
while IFS= read -r -d '' f; do
    gcc -std=c99 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -fsyntax-only "${inc[@]}" "$f"
    count=$((count+1))
done < <(find project/source/src project/source/legacy_mu1005/airplay project/source/legacy_mu1005/bluetooth -maxdepth 1 -name '*.c' -print0)
test "$count" -eq 45
printf 'R102 HOST STRICT COMPILE: %d/45 source modules PASS\n' "$count"
