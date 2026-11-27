#!/usr/bin/env python3
"""Compare strong ELF imports against dynamic exports of actual staged modules.
Static candidate providers only; never claim loader order or ABI compatibility."""
import concurrent.futures,json,re,subprocess,sys
from pathlib import Path

stage=Path(sys.argv[1]);sdk=Path(sys.argv[2])/"sdp/target/qnx6/armle-v7"
report=Path(sys.argv[3])
names=("libc.so.3","libm.so.2","libsocket.so.3")
def elf(p):
    try:
        with p.open("rb") as f:return f.read(6)==b"\x7fELF\x01\x01"
    except OSError:return False
files={p.name:p for p in stage.iterdir() if p.is_file() and elf(p)}
for n in names:
    for d in ("lib","usr/lib"):
        p=sdk/d/n
        if p.is_file() and elf(p):
            files.setdefault(n,p);break
def symbols(item):
    n,p=item
    raw=subprocess.check_output(["readelf","--dyn-syms","-W",str(p)],text=True,timeout=30)
    imports=set();exports=set()
    for line in raw.splitlines():
        a=line.split()
        if len(a)<8 or not re.fullmatch(r"\d+:",a[0]):continue
        bind,vis,ndx,name=a[4:8]
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.$@+-]{0,240}",name):continue
        if ndx=="UND" and bind=="GLOBAL":imports.add(name)
        elif ndx!="UND" and bind in ("GLOBAL","WEAK") and vis in ("DEFAULT","PROTECTED"):exports.add(name)
    return n,(imports,exports)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    syms=dict(pool.map(symbols,sorted(files.items())))
rows={}
for target in ("ipod-drvr-iap2.so","ipod-transport-usbdevice.so","mm-ipod"):
    imports,_=syms[target]
    owner=[];other=[];missing=[]
    for name in sorted(imports):
        providers=sorted(n for n,(_,exports) in syms.items() if n!=target and name in exports)
        if "mm-ipod" in providers and target!="mm-ipod":owner.append({"symbol":name,"providers":providers})
        elif providers:other.append({"symbol":name,"providers":providers})
        else:missing.append(name)
    rows[target]={"strong_import_count":len(imports),"mm_ipod_export_candidates":owner,"other_provider_count":len(other),"unresolved_exact_name_candidates":missing}
result={"scope":"STATIC_EXACT_SYMBOL_NAME_ONLY_NOT_ABI_OR_RUNTIME","sdk_baseline_names":[n for n in names if n in files],"targets":rows}
report.write_text(json.dumps(result,indent=2)+"\n")
for n,v in rows.items():print("IPOD_SYMBOL_PROVIDER_COUNTS",n,v["strong_import_count"],len(v["mm_ipod_export_candidates"]),len(v["unresolved_exact_name_candidates"]))
