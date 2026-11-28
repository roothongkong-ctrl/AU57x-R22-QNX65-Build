#!/usr/bin/env python3
"""Audit genuine donor dynamic imports, initializers and dependencies.
No emulation changes, binary patching, identity or authentication bypass."""
from pathlib import Path
import sys,json,re,subprocess,hashlib
root=Path(sys.argv[1]);retained=Path(sys.argv[2]);out=Path(sys.argv[3])
files=[root/"profiles/AID10.5-17PM"/n for n in ("wireless-receiver-exec","wcp_receiver","libomonob-wireless-maps.so")]+[root/"common/runtime/libv75-transport.so"]
def readelf(p,*args):
    return subprocess.check_output(["readelf",*args,str(p)],text=True)
def sym(t):
    rows=[];active=False
    for line in t.splitlines():
        if line.startswith("Symbol table "):active="'.dynsym'" in line;continue
        if not active:continue
        a=line.split()
        if len(a)>=8 and re.fullmatch(r"\d+:",a[0]) and a[4] in ("GLOBAL","WEAK") and a[5] in ("DEFAULT","PROTECTED"):
            rows.append({"name":a[7],"binding":a[4],"type":a[3],"undefined":a[6]=="UND"})
    return rows
providers={}
for n in ("libc.so.3","libsocket.so.3","libm.so.2"):
    providers[n]=sym((retained/(n+".txt")).read_text())
for f in files:
    providers[f.name]=sym(readelf(f,"--dyn-syms","-W"))
def match(a,b):
    return a.replace("@@","@")==b.replace("@@","@") or ("@" not in a and b.startswith(a+"@@"))
rows=[]
for f in files:
    dyn=readelf(f,"-dW");rel=readelf(f,"-rW");needed=re.findall(r"Shared library: \[([^\]]+)\]",dyn)
    # The two companion libraries are possible providers, not proof of actual preload order.
    scope=set(needed)|{"libomonob-wireless-maps.so","libv75-transport.so"}
    scope.discard(f.name)
    missing=[];cross=[]
    for s in providers[f.name]:
        if not s["undefined"]:continue
        hits=[n for n in sorted(scope) if any(not x["undefined"] and match(s["name"],x["name"]) for x in providers.get(n,[]))]
        if not hits:
            missing.append({**s,"relocations":[l.strip() for l in rel.splitlines() if re.search(r"(?<![\w])"+re.escape(s["name"].split("@")[0])+r"(?![\w])",l)]})
        elif not set(hits)&set(needed):cross.append({"symbol":s["name"],"candidate_providers":hits})
    row={"file":f.name,"sha256":hashlib.sha256(f.read_bytes()).hexdigest(),"needed":needed,"unknown_needed":sorted(set(needed)-providers.keys()),"unresolved_strong":[x for x in missing if x["binding"]=="GLOBAL"],"unresolved_weak":[x for x in missing if x["binding"]=="WEAK"],"companion_only":cross,"initializer_tags":[l.strip() for l in dyn.splitlines() if re.search(r"\((?:PREINIT_ARRAY|INIT_ARRAY|INIT|FINI_ARRAY|FINI|FLAGS|FLAGS_1|RPATH|RUNPATH)\)",l)]}
    rows.append(row)
    print("DONOR_RUNTIME_DEPENDENCIES",json.dumps(row))
out.write_text(json.dumps({"rows":rows,"scope":"static candidates; not actual loader binding or crash cause","installable":False},indent=2)+"\n")
