#!/usr/bin/env python3
"""Extend locked upstream runner with harmless QNX shell and fixture files."""
import sys
from pathlib import Path
p=Path(sys.argv[1]);s=p.read_text()
anchor='cp "$BIN" "$WORK/$NAME"'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\ncp "$STEP7_FIXTURE/step7-target" "$STEP7_FIXTURE/step7-wrap" "$WORK/"')
anchor='devc-serdebug\n$NAME\nEOF'
assert s.count(anchor)==1
s=s.replace(anchor,'devc-serdebug\n$NAME\n/bin/sh=step7-shell\nstep7-target\nstep7-wrap\nEOF')
anchor='export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib'
assert s.count(anchor)==1
s=s.replace(anchor,'if [ -f "$T/bin/sh" ]; then cp -L "$T/bin/sh" /w/step7-shell; elif [ -f "$T/bin/ksh" ]; then cp -L "$T/bin/ksh" /w/step7-shell; else echo SDK_SHELL_MISSING >&2; exit 1; fi\n'+anchor)
p.write_text(s)
