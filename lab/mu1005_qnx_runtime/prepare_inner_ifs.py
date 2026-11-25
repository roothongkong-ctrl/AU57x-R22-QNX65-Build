"""Extend the pinned QNX launcher with its real socket manager and one exact library."""
from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1: raise SystemExit("unexpected upstream anchor: "+a)
    s=s.replace(a,b)
replace('cp "$BIN" "$WORK/$NAME"', 'cp "$BIN" "$WORK/$NAME"\ncp "${QNX_TEST_INNER_LIB:?missing library}" "$WORK/libmu1005_inner_iap2_review.so"')
replace('    display_msg "@@QNXRUN-START@@"','    io-pkt-v4 &\n    waitfor /dev/socket 12\n    display_msg "@@QNXRUN-START@@"')
replace('devc-serdebug\n$NAME\nEOF','devc-serdebug\nio-pkt-v4\n[+raw +page_align perms=0555] libmu1005_inner_iap2_review.so\n$NAME\nEOF')
replace('export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib','''
# Resolve real SDK network manager dependencies, never stub them.
P=arm-unknown-nto-qnx6.5.0eabi
cp "$T/sbin/io-pkt-v4" /w/
pending="io-pkt-v4"
seen=" libc.so.3 libm.so.2 libsocket.so.3 "
while [ -n "$pending" ]; do
    next=""
    for exe in $pending; do
        for lib in $($P-readelf -d "/w/$exe" | sed -n "s/.*Shared library: \\[\\(.*\\)\\]/\\1/p"); do
            case "$seen" in *" $lib "*) continue;; esac
            case "$lib" in */*|..*) exit 1;; esac
            cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" || exit 1
            echo "$lib" >> /w/test.build
            seen="$seen$lib "
            next="$next $lib"
        done
    done
    pending="$next"
done
export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/sbin:$T/lib:$T/usr/lib'''.strip())
replace("if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then",'[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n'+"if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then")
p.write_text(s)
