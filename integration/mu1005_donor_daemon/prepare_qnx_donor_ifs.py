"""Stage test-only donor executable and real QNX socket manager in QEMU IFS."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
def rep(a,b):
 global s
 if s.count(a)!=1: raise SystemExit("unexpected qemu-run anchor: "+a)
 s=s.replace(a,b)
rep('cp "$BIN" "$WORK/$NAME"','''cp "$BIN" "$WORK/$NAME"
cp "${QNX_DONOR_LAB:?missing lab executable}" "$WORK/mu1005-donor-lab"
cp "${QNX_DONOR_DIO_FIXTURE:?missing lab configuration}" "$WORK/dio_manager.json"''')
rep('    display_msg "@@QNXRUN-START@@"','''    /proc/boot/slogger &
    waitfor /dev/slog 5
    /proc/boot/random -t &
    waitfor /dev/random 5
    CACHE_NOP=1 /proc/boot/io-pkt-v4-hc -v -ptcpip &
    waitfor /dev/socket 15
    /proc/boot/ifconfig lo0
    display_msg "@@QNXRUN-START@@"''')
rep('devc-serdebug\n$NAME\nEOF','''devc-serdebug
[+raw +page_align perms=0555] slogger
[+raw +page_align perms=0555] random
[+raw +page_align perms=0555] io-pkt-v4-hc
[+raw +page_align perms=0555] ifconfig
[+raw +page_align perms=0555] mu1005-donor-lab
[perms=0444] dio_manager.json
[+raw +page_align perms=0555] $NAME
[type=dir] /tmp
[type=dir] /var
[type=dir] /var/run
EOF''')
rep('export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib','''
P=arm-unknown-nto-qnx6.5.0eabi
cp "$T/sbin/io-pkt-v4-hc" "$T/sbin/ifconfig" "$T/usr/sbin/slogger" "$T/usr/sbin/random" /w/
pending="io-pkt-v4-hc ifconfig slogger random mu1005-donor-lab"
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
rep("if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then",'[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n'+"if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then")
p.write_text(s)
