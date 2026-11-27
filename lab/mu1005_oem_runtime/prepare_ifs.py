"""Add only hash-verified original libraries to disposable QNX boot image."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
def one(a,b):
    global s
    if s.count(a)!=1:raise ValueError("unexpected launcher anchor "+a)
    s=s.replace(a,b)
names=("libiap2client.so.1","libstrm.so.1","libdns_sd.so.1")
one('cp "$BIN" "$WORK/$NAME"', 'cp "$BIN" "$WORK/$NAME"\n'+'\n'.join('cp "\${QNX_OEM_PRIVATE:?missing private source}/'+n+'" "$WORK/'+n+'"' for n in names).replace("\\$","$"))
one("devc-serdebug\n$NAME\nEOF","devc-serdebug\n"+"\n".join("[+raw +page_align perms=0555] "+n for n in names)+"\n$NAME\nEOF")
p.write_text(s)
