#!/usr/bin/env python3
"""Find OEM runtime by filename, SONAME and same-image metadata, including sectionless ELF."""
import hashlib,json,re,subprocess,sys,struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
from mu1005_oem_inventory import metadata_aliases
root=Path(sys.argv[1]).resolve();sdk=Path(sys.argv[2]);out=Path(sys.argv[3])
wanted={"libc.so.3","libsocket.so.3","libm.so.2"}
def readelf(p,*flags):
    r=subprocess.run(["readelf",*flags,str(p)],text=True,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,timeout=20)
    return r.stdout
def exports(t):
    active=False;found=set()
    for l in t.splitlines():
        if l.startswith("Symbol table "):
            active="'.dynsym'" in l or "image" in l.lower()
            continue
        a=l.split()
        if not active and not re.fullmatch(r"\s*\d+:.*",l):continue
        if len(a)>=8 and re.fullmatch(r"\d+:",a[0]) and a[4] in ("GLOBAL","WEAK") and a[5] in ("DEFAULT","PROTECTED") and a[6]!="UND":
            found.add(a[7])
    return found
# SDK tables include .dynsym and possibly other tables: isolate .dynsym strictly.
def sdkexports(t):
    a=t.split("Symbol table '.dynsym'")
    if len(a)!=2:raise ValueError("missing SDK dynsym")
    return exports("Symbol table '.dynsym'"+a[1].split("\nSymbol table ",1)[0])
sdkrows={n:sdkexports((sdk/(n+".txt")).read_text()) for n in wanted}
rows={n:[] for n in wanted};links=[];seen=set();errors=[]
def inspect(p,alias=None):
    if p.is_symlink() or not p.is_file():return
    with p.open("rb") as f:h=f.read(52)
    if len(h)<52 or h[:6]!=b"\x7fELF\x01\x01" or struct.unpack_from("<H",h,18)[0]!=40:return
    dyn=readelf(p,"-dW")
    soname=re.findall(r"\(SONAME\).*?\[([^\]]+)\]",dyn)
    nms=({p.name}|set(soname)|({alias.name} if alias else set()))&wanted
    if not nms:return
    syms=exports(readelf(p,"-D","-sW"))
    sha=hashlib.sha256(p.read_bytes()).hexdigest()
    for n in sorted(nms):
        k=(n,str(p),str(alias))
        if k in seen:continue
        seen.add(k)
        r={"file":str(p.relative_to(root)),"alias":str(alias.relative_to(root)) if alias else None,"sha256":sha,"soname":soname,"export_count":len(syms),"sdk_export_count":len(sdkrows[n]),"only_oem":sorted(syms-sdkrows[n]),"only_sdk":sorted(sdkrows[n]-syms),"sectionless":struct.unpack_from("<I",h,32)[0]==0,"needed":re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",dyn)}
        rows[n].append(r)
        print("OEM_RUNTIME_CANDIDATE",n,json.dumps({k:v for k,v in r.items() if k not in ("only_oem","only_sdk")}),"oem_only",len(r["only_oem"]),"sdk_only",len(r["only_sdk"]))
for p in root.rglob("*"):
    if not p.is_file() or p.is_symlink():continue
    if p.name.endswith(".img") or p.suffix==".imagefs":continue
    try:
        # Scan all extracted ARM ELF files for SONAME, not just expected filenames.
        inspect(p)
    except (OSError,subprocess.SubprocessError,ValueError) as e:errors.append({"path":str(p.relative_to(root)),"error":str(e)})
tree=root/"stage2/filesystems/stage2";meta=root/"stage2/metadata/stage2.json"
if meta.is_file() and tree.is_dir():
    for l,t in metadata_aliases(tree,meta).items():
        if l.name in wanted:inspect(t,l)
    for e in json.loads(meta.read_text())["entries"]:
        if e.get("type")=="symlink" and Path(e.get("path","")).name in wanted:
            links.append({"path":e.get("path"),"target":e.get("target")})
report={"libraries":rows,"stage2_links":links,"errors":errors,"missing":[n for n,v in rows.items() if not v],"scope":"offline app70 and stage2/EFS; no OEM loader execution, no runtime compatibility proof","installable":False}
out.write_text(json.dumps(report,indent=2)+"\n")
print("OEM_RUNTIME_SUMMARY",json.dumps({"counts":{n:len(v) for n,v in rows.items()},"missing":report["missing"],"stage2_links":links,"error_count":len(errors)}))
