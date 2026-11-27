#!/usr/bin/env python3
import json,re,subprocess,sys
from pathlib import Path
p,h,j=map(Path,sys.argv[1:])
dyn=subprocess.check_output(["readelf","-dW",str(p)],text=True)
names=list(dict.fromkeys(re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",dyn)))
assert 0<len(names)<=100 and all(re.fullmatch(r"[A-Za-z0-9_.+-]+",n) for n in names)
h.write_text("static const char *const deps[]={"+",".join(json.dumps(n) for n in names)+",0};\n")
j.write_text(json.dumps({"program":p.name,"direct_needed":names,"scope":"ISOLATED_DLOPEN_ONLY"},indent=2)+"\n")
