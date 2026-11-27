#!/usr/bin/env python3
"""Original radio and launch candidates; dynamic-provider scope from prior evidence."""
import json,sys,re,hashlib,subprocess
from pathlib import Path
from mu1005_oem_inventory import PIN,inventory,sha
from mu1005_original_symbol_providers import symbols

def provider_scope(inv,slog):
    bt=[x for x in inv["files"]["btstack"] if x.get("matches_pinned") and x["sha256"]==PIN["btstack"]]
    assert len(bt)==1
    pending=[(n,[n]) for n in bt[0]["elf"]["needed"]]
    seen=set();matches=[];missing=[]
    exports={x["sha256"]:x["path"] for x in slog["symbol_results"] if x["export_candidate"]}
    while pending:
        name,chain=pending.pop(0)
        if name in seen:continue
        seen.add(name);entries=inv["dependency_candidates"].get(name,[])
        if not entries:missing.append(name)
        for x in entries:
            if x["sha256"] in exports:
                matches.append({"chain":chain,"provider":x["resolved_path"],"sha256":x["sha256"]})
            pending.extend((d,chain+[d]) for d in x["needed"] if d not in seen)
    return {"reachable_slogf_export_candidates":matches,"unresolved_in_original_graph":sorted(missing),"names_visited":len(seen),"all_export_candidates":len(exports),"loader_order_verified":False}

def files(root):
    for f in sorted(root.rglob("*")):
        if f.is_symlink() or not f.is_file() or not f.resolve().is_relative_to(root):continue
        rel=f.relative_to(root)
        if "filesystems" not in rel.parts or f.name.startswith("_") or f.stat().st_size>32*1024*1024:continue
        yield f,rel

def radio(root):
    rows=[]
    patterns=re.compile(r"wlan|uap|wif[iy]|80211|802_11|5ghz|2[._]4ghz|hostap|channel|regulatory",re.I)
    for f,rel in files(root):
        if not re.search(r"wlan|wifi|wpa|hostap|uap|80211",f.name,re.I):continue
        with f.open("rb") as h:
            if h.read(6)!=b"\x7fELF\x01\x01":continue
        try:
            inv=inventory(f);syms=symbols(f)
        except (ValueError,OSError,subprocess.SubprocessError):continue
        names=sorted({n for b,v,i,n in syms if patterns.search(n) and b in ("GLOBAL","WEAK")})
        raw=f.read_bytes()
        markers=[m.decode() for m in (b"uap0",b"wlan0",b"5GHz",b"2.4GHz",b"802.11a",b"802.11ac",b"hostap") if m in raw]
        rows.append({"path":str(rel),"sha256":sha(f),"needed":inv["needed"],"radio_symbol_candidates":names,"literal_markers":markers})
    return {"drivers":rows,"hardware_5ghz_verified":False,"ap_mode_runtime_verified":False}

def startup(root):
    rows=[]
    targets=(b"connectivity.json",b"bluetooth.enableIap",b"connectivity_launcher",b"devnp-mrvl_wlan",b"uap0")
    for f,rel in files(root):
        if f.stat().st_size>2*1024*1024:continue
        data=f.read_bytes()
        if data.startswith(b"\x7fELF") or b"\x00" in data:continue
        hits=[x.decode() for x in targets if x in data]
        if hits:rows.append({"path":str(rel),"sha256":sha(f),"references":hits})
    return {"text_reference_candidates":rows,"configuration_precedence_verified":False}

def main():
    lane=sys.argv[1]
    if lane=="provider-scope":
        a,b,o=map(Path,sys.argv[2:]);r=provider_scope(json.loads(a.read_text()),json.loads(b.read_text()))
    else:
        root,o=map(Path,sys.argv[2:]);root=root.resolve(strict=True)
        r=radio(root) if lane=="wifi-driver" else startup(root)
    r.update(lane=lane,runtime_verified=False,installable=False)
    with o.open("x") as f:json.dump(r,f,indent=2)
    print("R105105_EVIDENCE",lane,json.dumps(r))
if __name__=="__main__":main()
