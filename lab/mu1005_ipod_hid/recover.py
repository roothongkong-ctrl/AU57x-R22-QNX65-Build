#!/usr/bin/env python3
"""Recover exact real HID exports; never create dummy functions or alter OEM files."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
stage=Path(sys.argv[2]).resolve(strict=True)
old=json.loads(Path(sys.argv[3]).read_text())
report=Path(sys.argv[4])
wanted={"hid_remote_voldown","hid_remote_volup"}
assert set(old["targets"]["ipod-drvr-iap2.so"]["unresolved_exact_name_candidates"])==wanted
assert len(old["targets"]["ipod-drvr-iap2.so"]["mm_ipod_export_candidates"])==11
assert hashlib.sha256((stage/"ipod-drvr-iap2.so").read_bytes()).hexdigest()=="42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf"
def sha(p):
    h=hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
    return h.hexdigest()
def arm(p):
    with p.open("rb") as f:h=f.read(20)
    return len(h)==20 and h[:6]==b"\x7fELF\x01\x01" and h[18:20]==b"\x28\x00"
def run(p,*args):
    return subprocess.check_output(["readelf",*args,str(p)],text=True,stderr=subprocess.DEVNULL,timeout=30)
def dynamic(p):
    return run(p,"-dW")
def exports(p):
    hits=set()
    for line in run(p,"-D","-sW").splitlines():
        a=line.split()
        if len(a)>=8 and re.fullmatch(r"\d+:",a[0]) and a[7] in wanted and a[6]!="UND" and a[4] in ("GLOBAL","WEAK") and a[5] in ("DEFAULT","PROTECTED"):
            hits.add(a[7])
    return hits
rows=[];matches={};index={};seen=set();errors=0
for f in sorted(root.rglob("*")):
    if not f.is_file():continue
    p=f.resolve()
    if not p.is_relative_to(root) or not arm(p):continue
    if ".so" in f.name:index.setdefault(f.name,set()).add(p)
    if p in seen:continue
    seen.add(p)
    if p.stat().st_size>128*1024*1024:continue
    data=p.read_bytes()
    if not any(x.encode()+b"\0" in data for x in wanted):continue
    try:ex=exports(p)
    except (OSError,subprocess.SubprocessError):
        errors+=1;continue
    if not ex:continue
    digest=sha(p)
    shared=data[16:18]==b"\x03\x00"
    rows.append({"path":str(p.relative_to(root)),"sha256":digest,"exports":sorted(ex),"shared":shared})
    if ex==wanted and shared and ".so" in p.name:matches[digest]=p
result={"scope":"EXACT_ORIGINAL_HID_EXPORTS_ONLY","previous_host_exports":11,"candidates":rows,"scan_errors":errors,"runtime_candidate":False,"staged":[]}
def write():
    report.write_text(json.dumps(result,indent=2)+"\n")
    print("IPOD_HID_PROVIDER_RESULT",json.dumps(result,sort_keys=True))
if len(matches)!=1 or errors:
    result["reason"]="no_unique_complete_provider_or_scan_error"
    write();sys.exit(0)
p=next(iter(matches.values()))
alias="liblab-hid-provider.so"
pending=[(alias,p)];known={f.name for f in stage.iterdir()}|{"libc.so.3","libm.so.2","libsocket.so.3"}
selected={}
while pending:
    n,q=pending.pop(0)
    if n in known:continue
    if not re.fullmatch(r"[A-Za-z0-9_.+-]+",n):raise ValueError("unsafe dependency")
    if q is None:
        options={sha(x):x for x in sorted(index.get(n,set()))}
        if len(options)!=1:
            result["reason"]="unresolved_provider_dependency"
            result["dependency"]=n
            write();sys.exit(0)
        q=next(iter(options.values()))
    known.add(n);selected[n]=q
    pending.extend((d,None) for d in re.findall(r"\(NEEDED\).*?\[([^]]+)\]",dynamic(q)))
assert alias in selected
for n,q in selected.items():
    assert not (stage/n).exists()
    shutil.copyfile(q,stage/n)
    result["staged"].append({"name":n,"sha256":sha(q)})
result["runtime_candidate"]=True
result["reason"]="unique_real_provider_dependency_closure"
write()
