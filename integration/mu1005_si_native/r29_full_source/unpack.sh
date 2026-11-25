#!/bin/sh
set -eu
root=$(CDPATH= cd "$(dirname "$0")" && pwd)
out=${1:?destination required}
if [ -e "$out" ]; then echo 'destination already exists' >&2; exit 1; fi
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT HUP INT TERM
cat "$root"/source.part0 "$root"/source.part1 "$root"/source.part2 "$root"/source.part3 "$root"/source.part4 > "$tmp"
printf '%s  %s\n' '0e15785816b5adfdb643fb12f89fb97ef7db3883cd7e0abd4bedce1da961c821' "$tmp" | sha256sum -c -
mkdir -p "$out"
tar -xJf "$tmp" -C "$out"
test -f "$out/wcp_ipod_transport_v4_lab.c"
test -f "$out/build_qnx_review.sh"
