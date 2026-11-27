#!/usr/bin/env python3
"""Find original enableIap config candidates by content; never stage or execute."""
import argparse, hashlib, json
from pathlib import Path
from r99_stock_iap2_route import no_duplicates, stage_bluetooth_json

def scan(root):
    rows=[]
    for image in sorted(root.iterdir()):
        tree=image/"filesystems"
        if image.is_symlink() or not tree.is_dir():continue
        for f in sorted(tree.rglob("*")):
            if f.is_symlink() or not f.is_file() or not f.resolve().is_relative_to(root):continue
            if f.stat().st_size>1024*1024:continue
            data=f.read_bytes()
            if b"enableIap" not in data and f.name.lower()!="bluetooth.json":continue
            if b"\x00" in data:continue
            row={"path":str(f.relative_to(root)),"sha256":hashlib.sha256(data).hexdigest(),"bytes":len(data)}
            try:
                obj=json.loads(data.decode("utf-8-sig"),object_pairs_hook=no_duplicates)
                if not isinstance(obj,dict):raise ValueError("not a JSON object")
                keys=[]
                if "bluetooth.enableIap" in obj:keys.append(("dotted",obj["bluetooth.enableIap"]))
                if "enableIap" in obj and "topologyLogic" in obj:keys.append(("flat",obj["enableIap"]))
                if isinstance(obj.get("bluetooth"),dict) and "enableIap" in obj["bluetooth"]:keys.append(("nested",obj["bluetooth"]["enableIap"]))
                if len(keys)!=1 or type(keys[0][1]) is not bool:
                    row["status"]="unknown_or_ambiguous_schema"
                else:
                    row.update(schema=keys[0][0],enableIap=keys[0][1])
                    if keys[0][1]:row["status"]="already_enabled_candidate"
                    else:
                        try:
                            stage_bluetooth_json(data)
                            row["status"]="stageable_candidate_only"
                        except ValueError:
                            row["status"]="recognized_but_staging_rejected"
            except (UnicodeError,ValueError):
                row["status"]="non_json_or_invalid_candidate"
            rows.append(row)
    return rows

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("root",type=Path);p.add_argument("report",type=Path)
    a=p.parse_args();rows=scan(a.root.resolve(strict=True))
    result={"candidates":rows,"runtime_verified":False,"installable":False}
    with a.report.open("x") as f:json.dump(result,f,indent=2)
    print("OEM_CONFIG_DISCOVERY",len(rows))
    for row in rows:print(row["path"],row["status"])
    return 0
if __name__=="__main__":raise SystemExit(main())
