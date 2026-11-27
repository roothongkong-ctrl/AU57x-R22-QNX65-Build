#!/usr/bin/env python3
from pathlib import Path
import re,subprocess,sys
p=Path(sys.argv[1]);out=Path(sys.argv[2])
aliases=(out/"plugin_aliases.txt").read_text().splitlines()
(out/"plugin_aliases.txt").unlink()
src=Path(__file__).resolve().parents[1]/"mu1005_oem_launch"
subprocess.run([sys.executable,str(src/"prepare_ifs.py"),str(p),str(out),"layout"],check=True)
s=p.read_text();marker="\n$NAME\nEOF"
if s.count(marker)!=1:raise ValueError("manifest anchor")
extra=["[type=dir] /ramdisk","[type=dir] /ramdisk/pps","[type=dir] /ramdisk/pps/iap2"]
for line in aliases:
    a,module=line.split(" ",1)
    if module not in ("ipod-drvr-iap2.so","ipod-transport-usbdevice.so"):raise ValueError("unknown module")
    if not re.fullmatch(r"/(?:[A-Za-z0-9_.+-]+/)*[A-Za-z0-9_.+-]+",a) or ".." in a.split("/"):raise ValueError("invalid alias")
    extra.append("[type=link] "+a+"=/proc/boot/"+module)
s=s.replace(marker,"\n"+"\n".join(extra)+"\n[perms=0555]"+marker)
p.write_text(s)
