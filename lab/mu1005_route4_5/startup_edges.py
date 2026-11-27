#!/usr/bin/env python3
"""New boot-entry search using exact retained stock EFS; no OEM contents exported."""
import hashlib,json,re,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
need={"framework.json","dsistartup.json","reset_bt.sh","connectivity.json"}
rows=[]
for p in sorted(root.rglob("*")):
    if p.name not in need or p.is_symlink() or not p.is_file() or not p.resolve().is_relative_to(root):continue
    data=p.read_bytes()
    if len(data)>2_000_000:continue
    text=data.decode("utf-8","replace")
    anchors=["connectivity_launcher","btstack","bluetooth","/tmp/mvloaded","reset_bt.sh","on_startup_normal","set_app_runmode"]
    # Only anchor names and line numbers, never raw config contents or secrets.
    hits={a:[i for i,l in enumerate(text.splitlines(),1) if a in l] for a in anchors}
    rows.append({"name":p.name,"path":str(p.relative_to(root)),"sha256":digest(p),"anchors":{k:v for k,v in hits.items() if v}})
res={"scope":"STOCK_BOOT_ANCHOR_INVENTORY_ONLY","files":rows,"missing":sorted(need-{r["name"] for r in rows}),"factory_launch_verified":False,"installable":False}
out.write_text(json.dumps(res,indent=2)+"\n")
for r in rows:print("ROUTE6_BOOT_ANCHORS",json.dumps(r))
if not any(r["name"]=="connectivity.json" and r["sha256"]=="56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470" for r in rows):raise ValueError("exact config missing")
print("ROUTE6_STOCK_BOOT_SEARCH_ONLY_NOT_FACTORY_LAUNCH")
