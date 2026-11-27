#!/usr/bin/env python3
"""Stage exact pinned AirPlay closure; preserve ambiguous variants and missing names."""
import hashlib,json,re,shutil,sys,subprocess
from pathlib import Path

ROOT=Path(sys.argv[1]).resolve(strict=True)
report=json.loads(Path(sys.argv[2]).read_text())
out=Path(sys.argv[3])
evidence=Path(sys.argv[4])
safe=re.compile(r"^[A-Za-z0-9_.+-]+$")
base={"libc.so.3","libm.so.2","libsocket.so.3"}
def check(row):
    path=(ROOT/row.get("resolved_path",row["relative_path"])).resolve(strict=True)
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise ValueError("outside original tree")
    if hashlib.sha256(path.read_bytes()).hexdigest()!=row["sha256"]:
        raise ValueError("OEM hash mismatch "+path.name)
    return path
def dyn(path):
    s=subprocess.check_output(["readelf","-dW",str(path)],text=True,timeout=45)
    return sorted(set(re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",s)))
root_candidates=[r for r in report["files"]["libairplay.so"] if r.get("matches_pinned")]
if len(root_candidates)!=1: raise ValueError("AirPlay pin not unique")
root=root_candidates[0]
if root["sha256"]!="193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5":
    raise ValueError("AirPlay pin mismatch")
queue=["libairplay.so"]
choices={}
missing=[]
edges={}
while queue:
    name=queue.pop(0)
    if not safe.fullmatch(name):raise ValueError("unsafe dependency name")
    if name in choices or name in missing or name in base:continue
    rows=[root] if name=="libairplay.so" else report["dependency_candidates"].get(name,[])
    unique={}
    for row in rows:
        p=check(row)
        needs=dyn(p)
        unique.setdefault(row["sha256"],(p,needs))
        queue.extend(needs)
    if not unique:missing.append(name);continue
    choices[name]=unique
    edges[name]={d:n for d,(_,n) in unique.items()}
ambiguous={n:sorted(v) for n,v in choices.items() if len(v)>1}
# Deliberately no guessed runtime priority. Test each candidate in isolation.
profiles=[{}]
for n,ds in sorted(ambiguous.items()):
    profiles=[dict(p,**{n:d}) for p in profiles for d in ds]
    if len(profiles)>4:raise ValueError("too many distinct original variants")
out.mkdir(mode=0o700)
manifest=[]
for i,p in enumerate(profiles):
    target=out/("profile"+str(i));target.mkdir(mode=0o700)
    selected={}
    for n,opts in sorted(choices.items()):
        d=p[n] if n in p else next(iter(opts))
        src,_=opts[d]
        shutil.copyfile(src,target/n)
        selected[n]={"sha256":d,"source":str(src.relative_to(ROOT))}
    manifest.append({"profile":target.name,"selected":selected})
result={"scope":"OEM_ELF_LOADER_EXPERIMENT_NOT_INSTALLABLE","sdk_boot_baseline":sorted(base),
        "missing_original_candidates":sorted(missing),"ambiguous_original_candidates":ambiguous,
        "edges":edges,"profiles":manifest}
evidence.write_text(json.dumps(result,indent=2)+"\n")
print("AIRPLAY_ORIGINAL_MISSING",sorted(missing))
print("AIRPLAY_PROFILES",len(profiles))
