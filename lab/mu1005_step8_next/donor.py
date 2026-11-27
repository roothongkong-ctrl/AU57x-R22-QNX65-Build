import json,re,sys
from pathlib import Path
root=Path(sys.argv[1])
base=root/"standalone/SD_ROOT"
paths=[]
for p in sorted(base.rglob("*")):
 if not p.is_file() or p.suffix not in (".sh",".md",".json",".py",".c",".cpp",".h"):continue
 if p.stat().st_size>2000000:continue
 s=p.read_text(errors="replace")
 tags={key:len(re.findall(pattern,s,re.I)) for key,pattern in {"connectivity_json":r"connectivity\.json","stock_parser":r"parseFileToArray|JSONReader","original_launcher":r"connectivity_launcher","original_btstack":r"/eso/bin/apps/btstack","read_only_merge":r"read.only transformation"}.items()}
 if any(tags.values()):paths.append({"path":str(p.relative_to(root)),"matches":tags})
print(json.dumps({"scope":"PINNED_DONOR_SOURCE_BOUNDARIES_ONLY","files":paths,"mu1005_verified":False,"donor_code_executed":False},sort_keys=True))
assert paths
