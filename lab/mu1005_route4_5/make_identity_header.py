#!/usr/bin/env python3
"""Build fixed candidate list from retained reference manifest."""
import json,re,sys
from pathlib import Path
from prepare_reference import NAMES
rows=json.loads(Path(sys.argv[1]).read_text())
if len(rows)!=10 or {r["name"] for r in rows}!=NAMES:raise ValueError("missing or duplicate reference")
lines=[]
for r in rows:
    n=r["name"];sha=r["sha256"];size=r["bytes"]
    if not re.fullmatch(r"[0-9a-f]{64}",sha) or type(size)!=int or not 0<size<=10000000:raise ValueError("invalid identity")
    candidates=list(r["candidate_paths"])
    if n=="mm-ipod":candidates.append("/usr/sbin/mm-ipod")
    if n=="ipod-drvr-iap2.so":candidates.append("/lib/dll/ipod-drvr-iap2.so")
    for p in dict.fromkeys(candidates):
        if not re.fullmatch(r"/(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+",p) or any(x in (".","..") for x in p.split("/")) or p.split("/")[-1]!=n:raise ValueError("unsafe candidate")
        lines.append(' {"%s","%s","%s",%dULL},'%(n,p,sha,size))
Path(sys.argv[2]).write_text("static const struct entry rows[]={\n"+"\n".join(lines)+"\n};\n")
print("ROUTE4_FIXED_CANDIDATES",len(lines))
