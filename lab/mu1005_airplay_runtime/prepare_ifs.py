#!/usr/bin/env python3
"""Inject one original profile into the disposable QNX IFS, never the vehicle."""
import re,sys
from pathlib import Path
p=Path(sys.argv[1]);folder=Path(sys.argv[2]);s=p.read_text()
names=sorted(x.name for x in folder.iterdir() if x.is_file())
if not names or any(not re.fullmatch(r"[A-Za-z0-9_.+-]+",n) for n in names):
    raise ValueError("invalid original staging profile")
def one(a,b):
    global s
    if s.count(a)!=1:raise ValueError("unexpected launcher anchor "+a)
    s=s.replace(a,b,1)
one('cp "$BIN" "$WORK/$NAME"', 'cp "$BIN" "$WORK/$NAME"\n'+"\n".join('cp "'+chr(36)+'{QNX_OEM_PRIVATE:?}/'+n+'" "$WORK/'+n+'"' for n in names))
one("devc-serdebug\n$NAME\nEOF","devc-serdebug\n"+"\n".join("[+raw +page_align perms=0555] "+n for n in names)+"\n$NAME\nEOF")
p.write_text(s)
