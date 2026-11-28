#!/usr/bin/env python3
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
a='cp "$BIN" "$WORK/$NAME"'
assert s.count(a)==1
s=s.replace(a,a+'\ncp "${QNX_DONOR_RECEIVER:?receiver missing}" "$WORK/wcp_receiver"')
a='\n$NAME\nEOF\n'
assert s.count(a)==1
s=s.replace(a,'\n$NAME\n[+raw +page_align perms=0555] wcp_receiver\nEOF\n')
# Install genuine companion files in the disposable IFS, preserving the
# author's launcher paths. Do not fabricate an OEM profile or identity.
a='cp "${QNX_DONOR_RECEIVER:?receiver missing}" "$WORK/wcp_receiver"'
assert s.count(a)==1
s=s.replace(a,a+'\ncp "${QNX_DONOR_STAGE:?stage missing}"/wireless-receiver-exec "${QNX_DONOR_STAGE}"/libomonob-wireless-maps.so "${QNX_DONOR_STAGE}"/libv75-transport.so "$WORK/"')
a='[+raw +page_align perms=0555] wcp_receiver'
assert s.count(a)==1
s=s.replace(a,a+'\n[+raw +page_align perms=0555] wireless-receiver-exec\n[+raw +page_align perms=0555] libomonob-wireless-maps.so\n[+raw +page_align perms=0555] libv75-transport.so\n[type=link] /mnt/app/carplaymaps/wireless-receiver-exec=/proc/boot/wireless-receiver-exec\n[type=link] /mnt/app/carplaymaps/libomonob-wireless-maps.so=/proc/boot/libomonob-wireless-maps.so\n[type=link] /mnt/app/carplaymaps/libv75-transport.so=/proc/boot/libv75-transport.so\n[type=link] /mnt/ota/wcp_receiver=/proc/boot/wcp_receiver')
a='-display none -serial stdio'
assert s.count(a)==1
s=s.replace(a,'-nic user,restrict=on -display none -serial stdio')
# Stop U-Boot autoboot even when early serial bytes are lost.
# Keep the known working default NIC topology, restrict external networking.
a="( printf '\\n'; sleep 4"
assert s.count(a)==1
s=s.replace(a,"( for _i in 1 2 3 4 5 6 7 8 9 10; do printf '\\n'; sleep 1; done")
# Preserve serial even when the guest does not finish the probe.
a='if ! grep -q \'@@QNXRUN-START@@\' "$LOG"; then'
assert s.count(a)==1
s=s.replace(a,'[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n'+a)
p.write_text(s)
