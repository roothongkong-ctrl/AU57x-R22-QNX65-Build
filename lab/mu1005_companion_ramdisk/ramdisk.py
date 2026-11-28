#!/usr/bin/env python3
"""Provision genuine disposable RAM-backed filesystem in the QNX VM only."""
import sys
from pathlib import Path
p=Path(sys.argv[1]);mode=sys.argv[2];s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1:raise ValueError("unexpected anchor "+a)
    s=s.replace(a,b)
if mode=="flash":
    names=["devf-ram","flashctl"]
    copies='cp "$T/sbin/devf-ram" "$T/usr/bin/flashctl" /w/'
    startup="""    /proc/boot/devf-ram -s0,4m,,,64k -v -r &
    waitfor /dev/fs0p0 15
    /proc/boot/flashctl -p /dev/fs0p0 -e -f -n /ramdisk -m"""
    extra=""
elif mode=="block":
    names=["devb-ram","dinit","mount","io-blk.so","cam-disk.so","fs-qnx4.so"]
    copies='cp "$T/sbin/devb-ram" "$T/sbin/dinit" "$T/bin/mount" "$T/lib/dll/io-blk.so" "$T/lib/dll/cam-disk.so" "$T/lib/dll/fs-qnx4.so" /w/'
    startup="""    /proc/boot/devb-ram disk name=labdummy ram capacity=28,nodinit blk memory=sysram,alloc=demand,cache=512k,ramdisk=10m,vnode=256,verbose=cf &
    waitfor /dev/ram0 15
    /proc/boot/dinit /dev/ram0
    /proc/boot/mount -tqnx4 /dev/ram0 /ramdisk"""
    extra="\n"+"\n".join("[type=link] /lib/dll/"+n+"=/proc/boot/"+n for n in names[3:])
else:raise ValueError("unknown RAM provider")
replace('    display_msg "@@QNXRUN-START@@"',startup+'\n    display_msg "@@QNXRUN-START@@"')
a='[type=link] /mnt/ota/wcp_receiver=/proc/boot/wcp_receiver'
replace(a,a+"\n[type=dir] /ramdisk\n"+"\n".join("[+raw +page_align perms=0555] "+n for n in names)+extra)
deps=r'''
P=arm-unknown-nto-qnx6.5.0eabi
pending="NAMES"
seen=" libc.so.3 libm.so.2 libsocket.so.3 NAMES "
while [ -n "$pending" ]; do
    next=""
    for exe in $pending; do
        for lib in $($P-readelf -d "/w/$exe" | sed -n "s/.*Shared library: \[\(.*\)\]/\1/p"); do
            case "$seen" in *" $lib "*) continue;; esac
            case "$lib" in */*|..*) exit 1;; esac
            cp "$T/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/usr/lib/$lib" "/w/$lib" 2>/dev/null || cp "$T/lib/dll/$lib" "/w/$lib" || exit 1
            echo "[+raw +page_align perms=0555] $lib" >> /w/test.build
            seen="$seen$lib "
            next="$next $lib"
        done
    done
    pending="$next"
done
'''.replace("NAMES"," ".join(names))
a='export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib'
replace(a,copies+"\n"+deps+a)
p.write_text(s)
