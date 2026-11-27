#!/usr/bin/env python3
"""Read-only inventory of extracted stock firmware; no inferred runtime alias."""
import hashlib,json,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2])
wanted={"mm-ipod","ipod-drvr-iap2.so","btstack","bluetooth","smartphone_integrator","connectivity_launcher","connectivity.json","iap2.cfg","dio_manager","dio_manager.json"}
rows=[]
for p in sorted(root.rglob("*")):
    if p.name not in wanted or p.is_symlink() or not p.is_file() or not p.resolve().is_relative_to(root):continue
    rows.append({"name":p.name,"archive_path":str(p.relative_to(root)),"sha256":hashlib.sha256(p.read_bytes()).hexdigest(),"bytes":p.stat().st_size})
result={"scope":"EXTRACTED_FIRMWARE_PATHS_ONLY","files":rows,"missing_names":sorted(wanted-{x["name"] for x in rows}),"runtime_path_verified":False,"installable":False}
out.write_text(json.dumps(result,indent=2)+"\n")
for row in rows: print("ROUTE4_STOCK_FILE",json.dumps(row))
if not {"mm-ipod","ipod-drvr-iap2.so","connectivity.json"}<={x["name"] for x in rows}:raise ValueError("required known original inventory incomplete")
print("ROUTE4_INVENTORY_COMPLETE_NOT_RUNTIME")
