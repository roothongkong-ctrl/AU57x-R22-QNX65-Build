#!/usr/bin/env bash
# The host executable is a synthetic ELF fixture, NEVER stock btstack.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
w=$(mktemp -d)
trap 'rm -rf "$w"; rm -f /tmp/au57x_r92_bt_preload.log' EXIT
cc=${CC:-gcc}
$cc -std=c99 -O2 -Wall -Wextra -Werror -pthread tests/test_r92_fake_btstack.c -o "$w/btstack"
hash=$(sha256sum "$w/btstack" | cut -d' ' -f1)
$cc -std=c99 -O2 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Werror -fPIC -shared -pthread \
 -DAU57X_R92_HOST_TEST=1 -DAU57X_R92_STOCK_SHA256='"'"$hash"'"' \
 project/source/legacy_mu1005/bluetooth/bt_preload_probe.c \
 tools/review_txn/sha256.c -ldl -o "$w/libprobe.so"
rm -f /tmp/au57x_r92_bt_preload.log
# A true btstack-named process must pass both program identity and SHA.
AU57X_R92_PROBE=1 AU57X_R92_HOST_TARGET="$w/btstack" LD_PRELOAD="$w/libprobe.so" "$w/btstack" > "$w/good.log"
grep -q 'fake-btstack-thread-forwarding-ok' "$w/good.log"
grep -q 'stock-image-pinned' /tmp/au57x_r92_bt_preload.log
[ "$(grep -c 'pthread_create' /tmp/au57x_r92_bt_preload.log)" = 2 ]
# R93 regression: same stock file hash but a completely DIFFERENT process.
# This MUST NOT claim it is stock btstack, regardless of forged environment.
rm -f /tmp/au57x_r92_bt_preload.log
AU57X_R92_PROBE=1 AU57X_R92_HOST_TARGET="$w/btstack" LD_PRELOAD="$w/libprobe.so" /usr/bin/true
grep -q 'wrong-process' /tmp/au57x_r92_bt_preload.log
! grep -q 'stock-image-pinned' /tmp/au57x_r92_bt_preload.log
# Rebuild with deliberately wrong signature and prove no thread tracing occurs.
$cc -std=c99 -O2 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Werror -fPIC -shared -pthread \
 -DAU57X_R92_HOST_TEST=1 -DAU57X_R92_STOCK_SHA256='"'"$(printf '%064d' 0)"'"' \
 project/source/legacy_mu1005/bluetooth/bt_preload_probe.c \
 tools/review_txn/sha256.c -ldl -o "$w/libbad.so"
rm -f /tmp/au57x_r92_bt_preload.log
AU57X_R92_PROBE=1 AU57X_R92_HOST_TARGET="$w/btstack" LD_PRELOAD="$w/libbad.so" "$w/btstack" > "$w/bad.log"
grep -q 'pin-reject' /tmp/au57x_r92_bt_preload.log
! grep -q 'pthread_create' /tmp/au57x_r92_bt_preload.log
# With probe disabled, original program's thread behavior remains identical.
rm -f /tmp/au57x_r92_bt_preload.log
AU57X_R92_PROBE=0 AU57X_R92_HOST_TARGET="$w/btstack" LD_PRELOAD="$w/libprobe.so" "$w/btstack" > "$w/disabled.log"
grep -q 'fake-btstack-thread-forwarding-ok' "$w/disabled.log"
test ! -e /tmp/au57x_r92_bt_preload.log
echo 'R93 preload host tests PASS: actual shared-library constructor; 2 intercepted/forwarded pthread_create calls; SHA rejection; opt-out.'
