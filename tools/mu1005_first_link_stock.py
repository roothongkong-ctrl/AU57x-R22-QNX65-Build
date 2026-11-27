#!/usr/bin/env python3
"""Build the original-config-first connection candidate from actual app70.
Only reports paths, digests and the single semantic change; no car writes.
"""
import argparse,json
from pathlib import Path
from r99_stock_iap2_route import (BLUETOOTH_SHA,IAP2_DRIVER_SHA,sha,prepare,
    stage_bluetooth_json,read_bounded)
def main():
    a=argparse.ArgumentParser()
    a.add_argument("root",type=Path);a.add_argument("out",type=Path);a.add_argument("report",type=Path)
    o=a.parse_args();root=o.root.resolve(strict=True)
    def unique(items,kind):
        if len(items)!=1:raise ValueError(f"{kind}: expected one candidate, found {len(items)}")
        return items[0]
    files=[p for p in root.rglob("*") if p.is_file() and not p.is_symlink()]
    def pinned(name,digest):
        return unique([p for p in files if p.name==name and sha(p.read_bytes())==digest],name)
    bt=pinned("bluetooth",BLUETOOTH_SHA)
    driver=pinned("iap2_driver",IAP2_DRIVER_SHA)
    cfg=unique([p for p in files if p.name=="iap2.cfg"],"iap2.cfg")
    candidates=[]
    for p in files:
        if p.name.lower()!="bluetooth.json":continue
        raw=read_bounded(p)
        try:
            stage_bluetooth_json(raw)
        except ValueError:continue
        candidates.append(p)
    bc=unique(candidates,"bluetooth.json with disabled OEM enableIap")
    result=prepare(bc,cfg,sha(read_bounded(bc)),sha(read_bounded(cfg)),bt,driver,o.out)
    result["source_paths"]={k:str(v.relative_to(root)) for k,v in
        (("bluetooth",bt),("iap2_driver",driver),("bluetooth_config",bc),("iap2_config",cfg))}
    o.report.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n")
    print("OEM_FIRST_LINK_CONFIG_CANDIDATE_GENERATED_NOT_RUNTIME_PROVEN")
if __name__=="__main__":main()
