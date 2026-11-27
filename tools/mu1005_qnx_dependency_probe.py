#!/usr/bin/env python3
"""Compare pinned OEM btstack dynamic imports against exact-name QNX SDK candidates.
Static symbol coverage only; no execution, no claim of OEM ABI compatibility.
"""
import hashlib,json,pathlib,re,subprocess,sys
from mu1005_bt_iap_path_audit import BT_SHA
oem,root,out=map(pathlib.Path,sys.argv[1:])
def readelf(p,opt):
    return subprocess.check_output(["readelf",opt,str(p)],text=True,stderr=subprocess.STDOUT)
def dyn(p):
    return re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",readelf(p,"-Wd"))
def syms(p):
    # -W required to avoid truncation; use separately combined flags.
    text=subprocess.check_output(["readelf","-W","--dyn-syms",str(p)],text=True)
    for line in text.splitlines():
        a=line.split()
        if len(a)>=8 and re.fullmatch(r"\d+:",a[0]):
            yield a[4],a[5],a[6],a[7]
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(oem)==BT_SHA,"OEM pin mismatch"
needed=dyn(oem)
imports={name for bind,vis,ndx,name in syms(oem) if ndx=="UND" and bind=="GLOBAL"}
weak={name for bind,vis,ndx,name in syms(oem) if ndx=="UND" and bind=="WEAK"}
providers={}
results={}
for name in needed:
    f=root/name
    if not f.is_file():
        results[name]={"status":"missing_from_sdk"}
        continue
    header=readelf(f,"-h")
    if not re.search(r"Machine:\s+ARM\s*$",header,re.M) or "ELF32" not in header:
        results[name]={"status":"not_arm_elf32"}
        continue
    soname=re.findall(r"\(SONAME\).*?\[([^\]]+)\]",readelf(f,"-Wd"))
    if soname!=[name]:
        results[name]={"status":"soname_mismatch","soname":soname}
        continue
    exported=set()
    for bind,vis,ndx,s in syms(f):
        if ndx=="UND" or bind not in ("GLOBAL","WEAK") or vis not in ("DEFAULT","PROTECTED"):continue
        exported.add(s.replace("@@","@"))
        if "@@" in s: exported.add(s.split("@@",1)[0])
    matched=imports&exported
    results[name]={"status":"sdk_candidate_only","sha256":sha(f),"direct_dependencies":dyn(f),"matched_strong_imports":len(matched)}
    for symbol in matched:providers.setdefault(symbol,[]).append(name)
trace="_ZN7tracing7Channel5logCVEsPKcz"
report={"oem_sha256":BT_SHA,"direct_dependencies":results,"strong_import_count":len(imports),"weak_import_count":len(weak),"matched_strong_import_count":len(providers),"unmatched_strong_import_count":len(imports-set(providers)),"trace_symbol_provider_candidates":providers.get(trace,[]),"oem_abi_verified":False,"runtime_verified":False}
out.write_text(json.dumps(report,indent=2)+"\n")
print("OEM_SDK_STATIC_DEPENDENCY_REPORT",json.dumps(report))
