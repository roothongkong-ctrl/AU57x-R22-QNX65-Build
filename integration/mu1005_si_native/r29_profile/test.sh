#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cc -std=c99 -Wall -Wextra -Werror -DWCP_QNX_LAB_ONLY -DWCP_LAB_BT_TYPE_CANDIDATE=3 wcp_bt_link_profile.c test_bt_link_profile.c -o "$tmp/test"
"$tmp/test"
for type in 0 1 2 4 255; do
 if cc -std=c99 -DWCP_QNX_LAB_ONLY -DWCP_LAB_BT_TYPE_CANDIDATE="$type" -c wcp_bt_link_profile.c -o "$tmp/wrong.o" 2>"$tmp/errors"; then exit 1;fi
 grep -q 'MU1005 identification accepts type 3' "$tmp/errors"
 echo "PASS rejects type $type"
done
if cc -std=c99 -DWCP_LAB_BT_TYPE_CANDIDATE=3 -c wcp_bt_link_profile.c -o "$tmp/wrong.o" 2>"$tmp/errors"; then exit 1;fi
grep -q 'not a vehicle-installable' "$tmp/errors"
echo 'PASS rejects build without lab opt-in'
