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
a='-display none -serial stdio'
assert s.count(a)==1
s=s.replace(a,'-nic none -display none -serial stdio')
# Preserve serial even when the guest does not finish the probe.
a='if ! grep -q \'@@QNXRUN-START@@\' "$LOG"; then'
assert s.count(a)==1
s=s.replace(a,'[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n'+a)
p.write_text(s)
