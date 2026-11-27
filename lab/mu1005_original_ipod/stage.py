#!/usr/bin/env python3
"""Extend the original mm-ipod profile with its exact dynamic driver."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2]);report=Path(sys.argv[3])
src=Path(__file__).resolve().parents[1]/"mu1005_oem_launch"
subprocess.run([sys.executable,str(src/"stage.py"),str(root),str(out),"mm-ipod",str(report)],check=True)
r=json.loads(report.read_text())
app=root/"app70/filesystems/MMX2/app/70/default/app.img"
pin="42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def elf(p):
    with p.open("rb") as f:b=f.read(20)
    return len(b)==20 and b[:6]==b"\x7fELF\x01\x01" and b[18:20]==b"\x28\x00"
def needs(p):
    raw=subprocess.check_output(["readelf","-dW",str(p)],text=True,timeout=30)
    return re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",raw)
hits=[];index={}
for f in root.rglob("*"):
    if not f.is_file() or not (".so" in f.name or f.name=="iap2_driver"):continue
    q=f.resolve(strict=True)
    if not q.is_relative_to(root) or not elf(q):continue
    index.setdefault(f.name,set()).add(q)
    if f.name in ("ipod-drvr-iap2.so","iap2_driver") and sha(q)==pin:hits.append(q)
if not hits:raise ValueError("original plugin pin missing")
plugin=sorted(set(hits))[0]
name="ipod-drvr-iap2.so"
shutil.copyfile(plugin,out/name)
r["selected"][name]={"sha256":pin,"source":str(plugin.relative_to(root)),"size":plugin.stat().st_size}
pending=list(needs(plugin))
seen=set(r["selected"])|{"libc.so.3","libm.so.2","libsocket.so.3"}
missing=set(r["missing"]);ambiguous=dict(r["ambiguous"])
while pending:
    n=pending.pop(0)
    if n in seen:continue
    seen.add(n)
    if not re.fullmatch(r"[A-Za-z0-9_.+-]+",n):raise ValueError("bad dependency name")
    opts={sha(p):p for p in sorted(index.get(n,set()))}
    if len(opts)!=1:
        if not opts:missing.add(n)
        else:ambiguous[n]=sorted(opts)
        continue
    d,p=next(iter(opts.items()))
    shutil.copyfile(p,out/n)
    r["selected"][n]={"sha256":d,"source":str(p.relative_to(root)),"size":p.stat().st_size}
    missing.discard(n);ambiguous.pop(n,None);pending.extend(needs(p))
aliases=[]
for h in sorted(set(hits)):
    if not h.is_relative_to(app):continue
    rel=h.relative_to(app).as_posix()
    if not re.fullmatch(r"[A-Za-z0-9_.+/-]+",rel):continue
    aliases.append("/mnt/app/"+rel)
    if rel.startswith("armle/usr/lib/"):aliases.append("/usr/lib/"+rel[len("armle/usr/lib/"):])
r.update(plugin_sha256=pin,plugin_aliases=sorted(set(aliases)),missing=sorted(missing),ambiguous=ambiguous,device_service_ready=False)
report.write_text(json.dumps(r,indent=2)+"\n")
(out/"plugin_aliases.txt").write_text("\n".join(r["plugin_aliases"])+"\n")
print("ORIGINAL_IAP2_PLUGIN_STAGED",pin,"missing",len(missing),"ambiguous",len(ambiguous))
