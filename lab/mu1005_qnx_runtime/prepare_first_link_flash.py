"""Provision a disposable QNX RAM-backed flash filesystem, without io-blk."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1:raise SystemExit("unexpected launcher anchor: "+a)
    s=s.replace(a,b)
replace('    display_msg "@@QNXRUN-START@@"','''    display_msg "R105105 FLASH RAM START: disposable system RAM only"
    /proc/boot/devf-ram -s0,4m,,,64k -v -r &
    waitfor /dev/fs0p0 15
    /proc/boot/flashctl -p /dev/fs0p0 -e -f -n /tmp -m
    /proc/boot/pidin ar
    /proc/boot/sloginfo
    display_msg "@@QNXRUN-START@@"''')
replace('[type=dir] /tmp','''[+raw +page_align perms=0555] devf-ram
[+raw +page_align perms=0555] flashctl
[type=dir] /tmp''')
replace('pending="io-pkt-v4-hc ifconfig slogger random pidin sloginfo"','''cp "$T/sbin/devf-ram" "$T/usr/bin/flashctl" /w/
pending="io-pkt-v4-hc ifconfig slogger random pidin sloginfo devf-ram flashctl"''')
replace('cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" || exit 1','cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/lib/dll/$lib" "/w/$lib" || exit 1')
p.write_text(s)
