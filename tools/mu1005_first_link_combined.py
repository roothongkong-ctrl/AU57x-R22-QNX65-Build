#!/usr/bin/env python3
"""Locate exact stock iAP2 route across extracted MMX images, without car writes."""
import argparse,hashlib,json
from pathlib import Path
from r99_stock_iap2_route import BLUETOOTH_SHA,IAP2_DRIVER_SHA,prepare,stage_bluetooth_json,read_bounded

def scan(base):
    root=base.resolve(strict=True)
    pins={BLUETOOTH_SHA:[],IAP2_DRIVER_SHA:[]}
    cfg={"bluetooth":{},"iap2":{}}
    for image in sorted(root.iterdir()):
        tree=image/"filesystems"
        if not tree.is_dir() or image.is_symlink():continue
        for f in sorted(tree.rglob("*")):
            if f.is_symlink() or not f.is_file() or not f.resolve().is_relative_to(root):continue
            size=f.stat().st_size
            if 52<=size<=3*1024*1024:
                with f.open("rb") as h:
                    header=h.read(20)
                    if header[:6]==b"\x7fELF\x01\x01" and header[18:20]==b"\x28\x00":
                        digest=hashlib.sha256(header+h.read()).hexdigest()
                        if digest in pins:pins[digest].append(f)
            if f.name.lower() not in ("bluetooth.json","iap2.cfg"):continue
            if size>1024*1024:continue
            data=read_bounded(f)
            if f.name.lower()=="bluetooth.json":
                try:stage_bluetooth_json(data)
                except ValueError:continue
                group="bluetooth"
            else:
                group="iap2"
            cfg[group].setdefault(hashlib.sha256(data).hexdigest(),[]).append(f)
    return pins,cfg

def choose(base,pins,cfg):
    def relative(paths):return [str(p.relative_to(base)) for p in paths]
    report={"bluetooth_elf":relative(pins[BLUETOOTH_SHA]),"iap2_elf":relative(pins[IAP2_DRIVER_SHA]),
            "config_candidates":{k:{h:relative(v) for h,v in g.items()} for k,g in cfg.items()}}
    errors=[]
    for name,d in (("bluetooth_elf",BLUETOOTH_SHA),("iap2_elf",IAP2_DRIVER_SHA)):
        if not pins[d]:errors.append("missing "+name)
    for name,g in cfg.items():
        if len(g)!=1:errors.append(f"{name}: {len(g)} distinct config hashes")
    report["blockers"]=errors
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("root",type=Path);p.add_argument("out",type=Path);p.add_argument("report",type=Path)
    a=p.parse_args();root=a.root.resolve(strict=True)
    pins,cfg=scan(root);report=choose(root,pins,cfg)
    report.update(runtime_verified=False,installable=False)
    if not report["blockers"]:
        bh,bpaths=next(iter(cfg["bluetooth"].items()))
        ih,ipaths=next(iter(cfg["iap2"].items()))
        report["staging"]=prepare(bpaths[0],ipaths[0],bh,ih,pins[BLUETOOTH_SHA][0],pins[IAP2_DRIVER_SHA][0],a.out)
    a.report.parent.mkdir(parents=True,exist_ok=True)
    with a.report.open("x") as f:json.dump(report,f,indent=2);f.write("\n")
    if report["blockers"]:
        print("ORIGINAL_ROUTE_BLOCKED",report["blockers"]);return 2
    print("COMBINED_ORIGINAL_ROUTE_STAGED_NOT_RUNTIME_VERIFIED");return 0
if __name__=="__main__":raise SystemExit(main())
