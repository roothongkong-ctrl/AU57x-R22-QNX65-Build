#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
w=$(mktemp -d); trap 'rm -rf "$w"' EXIT
CC=${CC:-gcc}
$CC -std=c99 -O2 -Wall -Wextra -Werror -pthread -ldl r143_smoke.c -o "$w/btstack"
hash=$(sha256sum "$w/btstack" | cut -d' ' -f1)
HASH_DEFINE="-DAU57X_R143_STOCK_SHA256=\"$hash\""
$CC -std=c99 -O2 -Wall -Wextra -Werror -fPIC -shared -pthread -DAU57X_R143_HOST_TEST=1 -DAU57X_R143_TEST_API=1 "$HASH_DEFINE" r143_preload_probe.c bt_thread_owner.c sha256.c -ldl -o "$w/libr143.so"
rm -f /tmp/au57x_r143_btstack_probe.log
AU57X_R143_PROBE=1 AU57X_R143_HOST_TARGET="$w/btstack" LD_PRELOAD="$w/libr143.so" "$w/btstack"
BAD_HASH=$(printf '%064d' 0)
BAD_DEFINE="-DAU57X_R143_STOCK_SHA256=\"$BAD_HASH\""
$CC -std=c99 -O2 -Wall -Wextra -Werror -fPIC -shared -pthread -DAU57X_R143_HOST_TEST=1 -DAU57X_R143_TEST_API=1 "$BAD_DEFINE" r143_preload_probe.c bt_thread_owner.c sha256.c -ldl -o "$w/libbad.so"
rm -f /tmp/au57x_r143_btstack_probe.log
R143_NEGATIVE=1 AU57X_R143_PROBE=1 AU57X_R143_HOST_TARGET="$w/btstack" LD_PRELOAD="$w/libbad.so" "$w/btstack"
grep -q 'event=process_reject' /tmp/au57x_r143_btstack_probe.log
! grep -q 'event=thread_name' /tmp/au57x_r143_btstack_probe.log
! grep -q 'event=pthread_create' /tmp/au57x_r143_btstack_probe.log
echo 'R143 HOST PASS: exact process hash pin, transparent pthread_create, origin/name correlation, wrong-hash fail-closed.'
