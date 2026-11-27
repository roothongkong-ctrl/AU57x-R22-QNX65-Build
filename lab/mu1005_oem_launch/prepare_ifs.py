#!/usr/bin/env python3
from pathlib import Path
import re,sys
p=Path(sys.argv[1]);folder=Path(sys.argv[2]);mode=sys.argv[3]
if mode not in ("baseline","model"):raise ValueError("mode")
s=p.read_text()
names=sorted(x.name for x in folder.iterdir() if x.is_file() and x.name not in ("profile.h","candidate.config","connectivity.json","iap2.cfg"))
if not names or any(not re.fullmatch(r"[A-Za-z0-9_.+-]+",n) for n in names):raise ValueError("profile")
def one(a,b):
 global s
 if s.count(a)!=1:raise ValueError("anchor "+a)
 s=s.replace(a,b,1)
cp="\n".join('cp "\${QNX_OEM_PRIVATE:?}/'+n+'" "$WORK/'+n+'"' for n in names).replace("\\$","$")
cp+='\ncp "\${QNX_OEM_PRIVATE:?}/'+("candidate.config" if mode=="model" else "connectivity.json")+'" "$WORK/connectivity.json"\ncp "\${QNX_OEM_PRIVATE:?}/iap2.cfg" "$WORK/iap2.cfg"'
cp=cp.replace("\\$","$")
one('cp "$BIN" "$WORK/$NAME"','cp "$BIN" "$WORK/$NAME"\n'+cp)
entries="\n".join("[+raw +page_align perms=0555] "+n for n in names)
entries+="\n[perms=0444] /etc/eso/production/connectivity.json=connectivity.json\n[perms=0444] /etc/mm/iap2.cfg=iap2.cfg"
entries+="\n[type=link] /mnt/system/etc/eso/production/connectivity.json=/etc/eso/production/connectivity.json\n[type=link] /mnt/system/etc/mm/iap2.cfg=/etc/mm/iap2.cfg"
one("devc-serdebug\n$NAME\nEOF","devc-serdebug\n"+entries+"\n$NAME\nEOF")
if mode=="model":one("    $NAME\n    display_msg","    $NAME model\n    display_msg")
one("-M virt -m 256","-M virt -m 512")
p.write_text(s)
