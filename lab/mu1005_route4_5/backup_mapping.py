#!/usr/bin/env python3
"""Explicit extracted MIB backup location mapping, not a backup extractor.
A mapping is supplied by the owner after extraction; filenames alone are not evidence."""
import json,sys
from pathlib import Path
from prepare_reference import NAMES
from backup_verify import verify
def check(root,reference,mapping):
    if not isinstance(reference,list) or len(reference)!=len(NAMES) or {r["name"] for r in reference}!=NAMES:raise ValueError("reference coverage")
    if not isinstance(mapping,dict) or set(mapping)!=NAMES or any(not isinstance(v,str) for v in mapping.values()) or len(set(mapping.values()))!=len(NAMES):raise ValueError("explicit unique backup mapping required")
    rows=[{"path":mapping[r["name"]],"sha256":r["sha256"],"bytes":r["bytes"]} for r in reference]
    report=verify(root,rows)
    report["scope"]="EXTRACTED_REFERENCE_FILES_ONLY"
    report["full_vehicle_backup_verified"]=False
    report["restore_verified"]=False
    report["installable"]=False
    return report
if __name__=="__main__":
    if len(sys.argv)!=4:raise SystemExit("usage: backup_mapping.py EXTRACTED_BACKUP TRUSTED_REFERENCE_JSON EXPLICIT_MAPPING_JSON")
    print(json.dumps(check(sys.argv[1],json.loads(Path(sys.argv[2]).read_text()),json.loads(Path(sys.argv[3]).read_text())),indent=2))
