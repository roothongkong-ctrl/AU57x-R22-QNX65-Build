"""Provision a writable /tmp in disposable QNX first-link QEMU only."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1:raise SystemExit("unexpected launcher anchor: "+a)
    s=s.replace(a,b)
replace('    display_msg "@@QNXRUN-START@@"','''    /proc/boot/devb-ram ram capacity=1 blk ramdisk=10m,cache=512k,vnode=256 &
    waitfor /dev/ram0 15
    /proc/boot/dinit /dev/ram0
    /proc/boot/mount -tqnx4 /dev/ram0 /tmp
    /proc/boot/mount
    display_msg "@@QNXRUN-START@@"''')
replace('[type=dir] /tmp','''[+raw +page_align perms=0555] devb-ram
[+raw +page_align perms=0555] dinit
[+raw +page_align perms=0555] mount
[+raw +page_align perms=0555] io-blk.so
[+raw +page_align perms=0555] cam-disk.so
[+raw +page_align perms=0555] fs-qnx4.so
[type=link] /lib/dll/io-blk.so=/proc/boot/io-blk.so
[type=link] /lib/dll/cam-disk.so=/proc/boot/cam-disk.so
[type=link] /lib/dll/fs-qnx4.so=/proc/boot/fs-qnx4.so
[type=dir] /tmp''')
replace('pending="io-pkt-v4-hc ifconfig slogger random pidin sloginfo"','''cp "$T/sbin/devb-ram" "$T/sbin/dinit" "$T/bin/mount" "$T/lib/dll/io-blk.so" "$T/lib/dll/cam-disk.so" "$T/lib/dll/fs-qnx4.so" /w/
pending="io-pkt-v4-hc ifconfig slogger random pidin sloginfo devb-ram dinit mount io-blk.so cam-disk.so fs-qnx4.so"''')
replace('seen=" libc.so.3 libm.so.2 libsocket.so.3 "','seen=" libc.so.3 libm.so.2 libsocket.so.3 io-blk.so cam-disk.so fs-qnx4.so "')
replace('cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" || exit 1','cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/lib/dll/$lib" "/w/$lib" || exit 1')
p.write_text(s)
