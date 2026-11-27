#!/usr/bin/env python3
"""Read-only symbol provenance of exact OEM dependency candidates."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
from mu1005_oem_inventory import PIN, sha

def symbols(path):
    text=subprocess.check_output(["readelf","-D","-sW",str(path)],text=True,stderr=subprocess.STDOUT,timeout=60)
    result=[]
    for line in text.splitlines():
        a=line.split()
        if len(a)>=8 and re.fullmatch(r"\d+:",a[0]):
            result.append((a[4],a[5],a[6],a[7]))
    if not result:raise ValueError("unreadable dynamic symbols: "+path.name)
    return result

def checked(root,relative,digest):
    p=root/relative
    real=p.resolve(strict=True)
    if not real.is_relative_to(root) or not real.is_file() or sha(real)!=digest:
        raise ValueError("unverified input: "+relative)
    return real

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("root",type=Path);p.add_argument("inventory",type=Path);p.add_argument("out",type=Path)
    a=p.parse_args();root=a.root.resolve(strict=True)
    report=json.loads(a.inventory.read_text())
    bt=[x for x in report["files"]["btstack"] if x.get("matches_pinned")]
    if len(bt)!=1:raise ValueError("exact btstack pin not unique")
    oem=checked(root,bt[0]["relative_path"],PIN["btstack"])
    strong={n for b,v,i,n in symbols(oem) if b=="GLOBAL" and i=="UND"}
    weak={n for b,v,i,n in symbols(oem) if b=="WEAK" and i=="UND"}
    direct=bt[0]["elf"]["needed"]
    providers={};details={};missing=[]
    for name in direct:
        candidates=report["dependency_candidates"].get(name,[])
        if not candidates:missing.append(name);continue
        entries=[]
        for c in candidates:
            f=checked(root,c["resolved_path"],c["sha256"])
            ex=set()
            for b,v,i,n in symbols(f):
                if i=="UND" or b not in ("GLOBAL","WEAK") or v not in ("DEFAULT","PROTECTED"):continue
                ex.add(n.replace("@@","@"))
                if "@@" in n:ex.add(n.split("@@",1)[0])
            matched=strong & ex
            for n in matched:providers.setdefault(n,[]).append(c["resolved_path"])
            entries.append({"path":c["resolved_path"],"sha256":c["sha256"],"matched_strong_imports":len(matched)})
        details[name]=entries
    trace="_ZN7tracing7Channel5logCVEsPKcz"
    r={"scope":"OEM_STATIC_ONLY","btstack_sha256":PIN["btstack"],"strong_import_count":len(strong),"weak_import_count":len(weak),"matched_strong_import_count":len(providers),"unmatched_strong_import_count":len(strong-set(providers)),"direct_dependencies_without_oem_candidate":missing,"direct_dependency_candidates":details,"trace_symbol_provider_candidates":providers.get(trace,[]),"multiple_provider_count":sum(len(set(v))>1 for v in providers.values()),"unmatched_strong_imports":sorted(strong-set(providers)),"runtime_verified":False,"installable":False}
    with a.out.open("x") as f:json.dump(r,f,indent=2)
    print("OEM_STATIC_STRONG_IMPORT_MATCH",len(providers),len(strong))
    print("TRACE_PROVIDER",r["trace_symbol_provider_candidates"])
    print("MISSING_DIRECT_OEM_CANDIDATES",missing)
    return 0
if __name__=="__main__":raise SystemExit(main())
