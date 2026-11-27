#!/usr/bin/env python3
"""Publish only classified QNX runtime linker failures, never original logs."""
import json,re,sys
from pathlib import Path
def classify(paths):
    result={"library_not_found":set(),"symbol_not_found":set(),"loader_failure":False,"missing_path":False,"permission_denied":False}
    lib=re.compile(r"(?<![A-Za-z0-9_.+-])(?:lib[A-Za-z0-9_+-]{1,80}\.so(?:\.[0-9]{1,8}){0,4})(?![A-Za-z0-9_.+-])")
    sym=re.compile(r"(?:unresolved symbol|undefined symbol|symbol not found)\s*[:=]?\s*['\"]?([A-Za-z_][A-Za-z0-9_]{0,200})",re.I)
    for p in paths:
        if not p.is_file():continue
        for line in p.read_text(errors="replace").splitlines():
            lower=line.lower()
            if any(x in lower for x in ("library cannot be found","cannot load library","could not load library","unable to load library","shared library not found","error loading shared library")):
                result["loader_failure"]=True
                result["library_not_found"].update(lib.findall(line))
            if any(x in lower for x in ("unresolved symbol","undefined symbol","symbol not found")):
                result["loader_failure"]=True
                result["symbol_not_found"].update(sym.findall(line))
            if "no such file or directory" in lower:result["missing_path"]=True
            if "permission denied" in lower:result["permission_denied"]=True
    return {k:sorted(v) if isinstance(v,set) else v for k,v in result.items()}
if __name__=="__main__":
    result=classify([Path(p) for p in sys.argv[2:]])
    Path(sys.argv[1]).write_text(json.dumps(result,indent=2)+"\n")
    print("QNX_LINKER_CLASSIFICATION",json.dumps(result,sort_keys=True))
