#!/usr/bin/env python3
"""Transform retained firmware inventory into a separate reference manifest.
Archive-derived paths are hypotheses, never verified vehicle write locations."""
import json,re,sys
from pathlib import Path,PurePosixPath
NAMES={"mm-ipod","ipod-drvr-iap2.so","btstack","bluetooth","smartphone_integrator","connectivity_launcher","connectivity.json","iap2.cfg","dio_manager","dio_manager.json"}
APP="/default/app.img/"
EFS="/default/efs-system.img/"
def prepare(data):
    if data.get("scope")!="EXTRACTED_FIRMWARE_PATHS_ONLY" or data.get("runtime_path_verified") is not False:raise ValueError("wrong reference scope")
    manifest=[]; seen=set()
    for row in data["files"]:
        name=row["name"]; src=row["archive_path"]; sha=row["sha256"]; size=row["bytes"]
        if name not in NAMES or name in seen or not re.fullmatch(r"[0-9a-f]{64}",sha) or type(size)!=int or size<1:raise ValueError("ambiguous reference")
        if src.startswith("app70/") and src.count(APP)==1:kind="app";rel=src.split(APP)[1]
        elif src.startswith("stage2/") and src.count(EFS)==1:kind="system";rel=src.split(EFS)[1]
        else:raise ValueError("unexpected filesystem")
        p=PurePosixPath(rel)
        if p.name!=name or p.is_absolute() or any(not re.fullmatch(r"[A-Za-z0-9_.-]+",x) or x in (".","..") for x in rel.split("/")):raise ValueError("unsafe reference path")
        seen.add(name)
        manifest.append({"name":name,"path":kind+"/"+rel,"sha256":sha,"bytes":size,"candidate_paths":["/"+rel,"/mnt/"+kind+"/"+rel]})
    if seen!=NAMES:raise ValueError("incomplete reference")
    return sorted(manifest,key=lambda x:x["name"])
if __name__=="__main__":
    rows=prepare(json.loads(Path(sys.argv[1]).read_text()))
    out=Path(sys.argv[2]);out.mkdir(exist_ok=True)
    (out/"reference_manifest.json").write_text(json.dumps(rows,indent=2)+"\n")
    (out/"candidate_paths.tsv").write_text("".join(r["sha256"]+" "+str(r["bytes"])+" "+p+"\n" for r in rows for p in r["candidate_paths"]))
    print("ROUTE4_5_REFERENCE_MANIFEST_NOT_LIVE",len(rows))
