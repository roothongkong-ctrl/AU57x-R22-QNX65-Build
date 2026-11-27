#!/usr/bin/env python3
"""Stage actual programs from pinned archive. No OEM artifacts are published."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2]); kind=sys.argv[3]; evidence=Path(sys.argv[4])
app=root/"app70/filesystems/MMX2/app/70/default/app.img"
efs=root/"stage2/filesystems/MMX2/efs-sys/70/default/efs-system.img"
roots={"btstack":"eso/bin/apps/btstack","bluetooth":"eso/bin/apps/bluetooth",
       "smartphone_integrator":"eso/bin/apps/smartphone_integrator","mm-ipod":"armle/usr/sbin/mm-ipod","dio_manager":"eso/bin/apps/dio_manager"}
pins={"bluetooth":"7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272",
      "dio_manager":"e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c"}
base={"libc.so.3","libm.so.2","libsocket.so.3"}
safe=re.compile(r"^[A-Za-z0-9_.+-]+$")
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def checked(p):
    p=p.resolve(strict=True)
    if not p.is_relative_to(root) or not p.is_file():raise ValueError("outside original tree")
    return p
def elf(p):
    with p.open("rb") as f:h=f.read(20)
    return len(h)==20 and h[:6]==b"\x7fELF\x01\x01" and h[18:20]==b"\x28\x00"
def needs(p):
    r=subprocess.run(["readelf","-dW",str(p)],capture_output=True,text=True,timeout=30)
    if r.returncode:raise ValueError("readelf failed: "+p.name)
    return re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",r.stdout)
def fnv(p):
    h=2166136261
    with p.open("rb") as f:
        for chunk in iter(lambda:f.read(65536),b""):
            for b in chunk:h=((h^b)*16777619)&0xffffffff
    return h
p=checked(app/roots[kind])
if not elf(p) or (kind in pins and sha(p)!=pins[kind]):raise ValueError("program identity")
index={}
for f in root.rglob("*"):
    if f.is_file() and (f.name.startswith("lib") or ".so" in f.name):
        try:
            q=checked(f)
            if elf(q):index.setdefault(f.name,set()).add(q)
        except (OSError,ValueError):continue
out.mkdir(mode=0o700)
selected={};missing=[];ambiguous={};queue=[(kind,p)]
done=set()
while queue:
    n,q=queue.pop(0)
    if n in done or n in base:continue
    if not safe.fullmatch(n):raise ValueError("unsafe name")
    done.add(n)
    if q is None:
        options=index.get(n,set())
        # Exact app path matches real process map location class, not a guessed other firmware.
        priorities=[app/"eso/lib"/n,app/"armle/usr/lib"/n,app/"root/lib-target"/n,app/"armle/lib"/n]
        exact={checked(x) for x in priorities if x.is_file() and elf(checked(x))}
        options=exact or options
        byhash={sha(x):x for x in sorted(options)}
        if len(byhash)!=1:
            if not byhash:missing.append(n)
            else:ambiguous[n]=sorted(byhash)
            continue
        q=next(iter(byhash.values()))
    shutil.copyfile(q,out/n)
    selected[n]={"sha256":sha(q),"source":str(q.relative_to(root)),"size":q.stat().st_size,"fnv32":fnv(q)}
    queue.extend((x,None) for x in needs(q))
# Exact config only; never synthesize authentication data.
cfg=checked(efs/"etc/eso/production/connectivity.json")
original=cfg.read_bytes()
if hashlib.sha256(original).hexdigest()!="56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470":raise ValueError("config hash")
lines=original.splitlines(keepends=True)
if lines[215].count(b"false")!=1:raise ValueError("config anchor")
lines[215]=lines[215].replace(b"false",b"true",1)
candidate=b"".join(lines)
if hashlib.sha256(candidate).hexdigest()!="75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5":raise ValueError("candidate hash")
(out/"connectivity.json").write_bytes(original)
(out/"candidate.config").write_bytes(candidate)
# Prefer actual EFS config path, never alter iAP2 config.
icfg=checked(efs/"etc/mm/iap2.cfg")
if sha(icfg)!="85039d16132d827ddcb53be82c84a2d6ec6f02be6ef33258fa33e181e443715b":raise ValueError("iap2 config pin")
shutil.copyfile(icfg,out/"iap2.cfg")
(out/"profile.h").write_text('#define PROGRAM "'+kind+'"\n#define PROGRAM_SIZE '+str(selected[kind]["size"])+'u\n#define PROGRAM_FNV '+str(selected[kind]["fnv32"])+'u\n')
evidence.write_text(json.dumps({"program":kind,"archive_pinned":True,"selected":selected,"missing":sorted(missing),"ambiguous":ambiguous,"sdk_baseline":sorted(base),"runtime_ready":False},indent=2)+"\n")
print("STAGED_ORIGINAL",kind,"missing",len(missing),"ambiguous",len(ambiguous))
