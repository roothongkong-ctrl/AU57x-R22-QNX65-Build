#!/usr/bin/env python3
"""Run actual R99 stage and verifier on pinned originals, privately."""
import json,sys,tempfile
from pathlib import Path
from r99_stock_iap2_route import prepare,sha,BLUETOOTH_SHA,IAP2_DRIVER_SHA
from r99_verify_stage import verify
from mu1005_efs_launcher_stage import CONFIG_SHA
from mu1005_config_schema import PREFIX

CFG_SHA="85039d16132d827ddcb53be82c84a2d6ec6f02be6ef33258fa33e181e443715b"

def pinned(root,name,digest):
    hits=[]
    for f in root.rglob(name):
        if f.is_symlink() or not f.is_file() or not f.resolve().is_relative_to(root):continue
        if f.stat().st_size>3*1024*1024:continue
        if sha(f.read_bytes())==digest:hits.append(f)
    if not hits:raise ValueError("missing pinned "+name)
    return sorted(hits)[0]

def run(root):
    config=(root/(PREFIX+"production/connectivity.json")).resolve(strict=True)
    if not config.is_relative_to(root):raise ValueError("invalid source path")
    iap=pinned(root,"iap2.cfg",CFG_SHA)
    bt=pinned(root,"bluetooth",BLUETOOTH_SHA)
    driver=pinned(root,"ipod-drvr-iap2.so",IAP2_DRIVER_SHA)
    with tempfile.TemporaryDirectory(prefix="mu1005-r99-private-") as t:
        dest=Path(t)/"stage"
        manifest=prepare(config,iap,CONFIG_SHA,CFG_SHA,bt,driver,dest)
        verified=verify(dest,CONFIG_SHA,CFG_SHA)
        f=dest/"bluetooth.STAGED_REVIEW_ONLY.json"
        raw=f.read_bytes()
        f.write_bytes(raw+b"\n")
        try:verify(dest,CONFIG_SHA,CFG_SHA)
        except ValueError:pass
        else:raise ValueError("tampered stage accepted")
        f.write_bytes(raw)
        verify(dest,CONFIG_SHA,CFG_SHA)
        assert sha(config.read_bytes())==CONFIG_SHA and sha(iap.read_bytes())==CFG_SHA
        return {"manifest":manifest,"verified":verified,"tamper_rejected":True,"source_unchanged":True,"raw_files_published":False,"runtime_verified":False,"installable":False}

if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True)
    r=run(root)
    with Path(sys.argv[2]).open("x") as f:json.dump(r,f,indent=2)
    print("R105109_ORIGINAL_STAGE",json.dumps(r))
