#!/usr/bin/env bash
# The host executable is a synthetic ELF fixture, NEVER stock btstack.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
w=$(mktemp -d)
trap 'rm -rf "$w"; rm -f /tmp/au57x_r92_bt_preload.log' EXIT
cc=${CC:-gcc}
$cc -std=c99 -O2 -Wall -Wextra -Werror -pthread tests/test_r92_fake_btstack.c -o "$w/fake-btstack"
hash=$(sha256sum "$w/fake-btstack" | cut -d' ' -f1)
$cc -std=c99 -O2 -Wall -Wextra -Werror -fPIC -shared -pthread \
 -DAU57X_R92_HOST_TEST=1 -DAU57X_R92_STOCK_SHA256='"'"$hash"'"' \
 project/source/legacy_mu1005/bluetooth/bt_preload_probe.c \
 tools/review_txn/sha256.c -ldl -o "$w/libprobe.so"
rm -f /tmp/au57x_r92_bt_preload.log
AU57X_R92_PROBE=1 AU57X_R92_HOST_TARGET="$w/fake-btstack" LD_PRELOAD="$w/libprobe.so" "$w/fake-btstack" > "$w/good.log"
grep -q 'fake-btstack-thread-forwarding-ok' "$w/good.log"
grep -q 'stock-image-pinned' /tmp/au57x_r92_bt_preload.log
[ "$(grep -c 'pthread_create' /tmp/au57x_r92_bt_preload.log)" = 2 ]
# Rebuild with deliberately wrong signature and prove no thread tracing occurs.
$cc -std=c99 -O2 -Wall -Wextra -Werror -fPIC -shared -pthread \
 -DAU57X_R92_HOST_TEST=1 -DAU57X_R92_STOCK_SHA256='"'"$(printf '%064d' 0)"'"' \
 project/source/legacy_mu1005/bluetooth/bt_preload_probe.c \
 tools/review_txn/sha256.c -ldl -o "$w/libbad.so"
rm -f /tmp/au57x_r92_bt_preload.log
AU57X_R92_PROBE=1 AU57X_R92_HOST_TARGET="$w/fake-btstack" LD_PRELOAD="$w/libbad.so" "$w/fake-btstack" > "$w/bad.log"
grep -q 'pin-reject' /tmp/au57x_r92_bt_preload.log
! grep -q 'pthread_create' /tmp/au57x_r92_bt_preload.log
# With probe disabled, original program's thread behavior remains identical.
rm -f /tmp/au57x_r92_bt_preload.log
AU57X_R92_PROBE=0 AU57X_R92_HOST_TARGET="$w/fake-btstack" LD_PRELOAD="$w/libprobe.so" "$w/fake-btstack" > "$w/disabled.log"
grep -q 'fake-btstack-thread-forwarding-ok' "$w/disabled.log"
test ! -e /tmp/au57x_r92_bt_preload.log
echo 'R92 preload host tests PASS: actual shared-library constructor; 2 intercepted/forwarded pthread_create calls; SHA rejection; opt-out.'
