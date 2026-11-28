"""Reuse the R105.111 QNX socket-manager bootstrap without unrelated fixture libraries."""
from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1: raise SystemExit("unexpected upstream anchor: "+a)
    s=s.replace(a,b)
replace('    display_msg "@@QNXRUN-START@@"','    /proc/boot/slogger &\n    waitfor /dev/slog 5\n    /proc/boot/random -t &\n    waitfor /dev/random 5\n    CACHE_NOP=1 /proc/boot/io-pkt-v4-hc -v -ptcpip &\n    waitfor /dev/socket 15\n    /proc/boot/pidin ar\n    /proc/boot/sloginfo\n    /proc/boot/ifconfig lo0\n    display_msg "@@QNXRUN-START@@"')
replace('devc-serdebug\n$NAME\nEOF','devc-serdebug\n[+raw +page_align perms=0555] slogger\n[+raw +page_align perms=0555] random\n[+raw +page_align perms=0555] io-pkt-v4-hc\n[+raw +page_align perms=0555] pidin\n[+raw +page_align perms=0555] sloginfo\n[+raw +page_align perms=0555] ifconfig\n$NAME\n[type=dir] /tmp\n[type=dir] /var\n[type=dir] /var/run\nEOF')
replace('export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib','''
# Resolve real SDK network manager dependencies, never stub them.
P=arm-unknown-nto-qnx6.5.0eabi
cp "$T/sbin/io-pkt-v4-hc" "$T/sbin/ifconfig" "$T/usr/sbin/slogger" "$T/usr/sbin/random" "$T/bin/pidin" "$T/bin/sloginfo" /w/
pending="io-pkt-v4-hc ifconfig slogger random pidin sloginfo"
seen=" libc.so.3 libm.so.2 libsocket.so.3 "
while [ -n "$pending" ]; do
    next=""
    for exe in $pending; do
        for lib in $($P-readelf -d "/w/$exe" | sed -n "s/.*Shared library: \\[\\(.*\\)\\]/\\1/p"); do
            case "$seen" in *" $lib "*) continue;; esac
            case "$lib" in */*|..*) exit 1;; esac
            cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" || exit 1
            echo "[+raw +page_align perms=0555] $lib" >> /w/test.build
            seen="$seen$lib "
            next="$next $lib"
        done
    done
    pending="$next"
done
export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/sbin:$T/lib:$T/usr/lib'''.strip())
replace("if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then",'[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n'+"if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then")
p.write_text(s)
