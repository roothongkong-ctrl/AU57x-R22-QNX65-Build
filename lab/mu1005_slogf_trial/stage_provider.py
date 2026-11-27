#!/usr/bin/env python3
"""Use real logging providers only; never replace auth or other OEM functions."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2]).resolve(strict=True)
mode=sys.argv[3]
report=Path(sys.argv[4])
name="liblogging.so.1" if mode=="oem" else "liblab-slogf.so"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def elf(p):
    with p.open("rb") as f:b=f.read(20)
    return b[:6]==b"\x7fELF\x01\x01" and b[16:20]==b"\x03\x00\x28\x00"
def dyn(p,flag):
    return subprocess.check_output(["readelf",*flag,str(p)],text=True,timeout=30)
def exports(p):
    for line in dyn(p,["-D","-sW"]).splitlines():
        a=line.split()
        if len(a)>=8 and a[7].split("@")[0]=="slogf" and a[6]!="UND" and a[4] in ("GLOBAL","WEAK") and a[5] in ("DEFAULT","PROTECTED"):return True
    return False
def needs(p):return re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",dyn(p,["-dW"]))
if mode=="oem":
    p=root/"app70/filesystems/MMX2/app/70/default/app.img/armle/usr/lib/liblogging.so.1"
    p=p.resolve(strict=True)
    assert p.is_relative_to(root) and sha(p)=="feb721dd0486ecb17bb9ff43efa5f5ff35bb9d1d58568b89fa4d13be9211598e"
else:
    assert mode=="sdk"
    p=Path(sys.argv[5]).resolve(strict=True)
assert elf(p) and exports(p),"provider lacks real ARM slogf export"
assert not (out/name).exists(),"refuse overwrite"
index={}
for f in root.rglob("*"):
    if not f.is_file() or ".so" not in f.name:continue
    q=f.resolve()
    if q.is_relative_to(root) and elf(q):index.setdefault(f.name,set()).add(q)
seen={"libc.so.3","libm.so.2","libsocket.so.3"}|{q.name for q in out.iterdir()}
pending=[(name,p)];staged={}
while pending:
    n,q=pending.pop(0)
    if n in seen:continue
    assert re.fullmatch(r"[A-Za-z0-9_.+-]+",n)
    if q is None:
        opts={sha(x):x for x in sorted(index.get(n,set()))}
        assert len(opts)==1,"missing or ambiguous provider dependency: "+n
        q=next(iter(opts.values()))
    shutil.copyfile(q,out/n)
    seen.add(n);staged[n]=sha(q)
    pending.extend((d,None) for d in needs(q))
report.write_text(json.dumps({"scope":"LAB_ONLY","provider":name,"staged_sha256":staged,"verified_slogf_export":True,"runtime_verified":False},indent=2)+"\n")
print("REAL_LOGGING_PROVIDER_STAGED",mode,name)
