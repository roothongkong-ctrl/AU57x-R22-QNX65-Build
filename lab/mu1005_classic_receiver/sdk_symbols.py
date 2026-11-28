#!/usr/bin/env python3
"""Check donor strong dynamic imports against retained QNX SDK symbol evidence.
Only static symbol closure, not calling-convention or actual vehicle proof."""
import json,re,subprocess,sys
from pathlib import Path
def symbols(text,undefined):
    found=set()
    # Only .dynsym: do not mistake non-exported .symtab entries for providers.
    active=False
    for l in text.splitlines():
        if l.startswith("Symbol table "):
            active="'.dynsym'" in l
            continue
        if not active:continue
        a=l.split()
        if len(a)<8 or not re.fullmatch(r"\d+:",a[0]):continue
        if a[4] not in ("GLOBAL","WEAK") or a[5] not in ("DEFAULT","PROTECTED"):continue
        if (a[6]=="UND")!=undefined:continue
        if undefined and a[4]!="GLOBAL":continue
        found.add(a[7])
    return found
def candidates(name,providers):
    if "@" in name:
        return sorted(x for x in providers if x.replace("@@","@")==name.replace("@@","@"))
    return sorted(x for x in providers if x==name or x.startswith(name+"@@"))
def main():
    root=Path(sys.argv[1]);donor=Path(sys.argv[2]);out=Path(sys.argv[3])
    libs=["libc.so.3","libsocket.so.3","libm.so.2"]
    provider={n:symbols((root/(n+".txt")).read_text(),False) for n in libs}
    if not all(provider.values()):raise ValueError("missing genuine dynamic symbol table")
    need=subprocess.check_output(["readelf","-dW",str(donor)],text=True)
    needed=re.findall(r"Shared library: \[([^\]]+)\]",need)
    if not needed or set(needed)-set(libs):raise ValueError("unexpected DT_NEEDED")
    imports=symbols(subprocess.check_output(["readelf","--dyn-syms","--wide",str(donor)],text=True),True)
    if not imports:raise ValueError("empty donor imports")
    unresolved=[];resolved={};ambiguous={}
    for x in sorted(imports):
        matches={n:candidates(x,provider[n]) for n in needed}
        matches={n:v for n,v in matches.items() if v}
        if not matches:unresolved.append(x)
        else:
            resolved[x]=matches
            if len(matches)>1:ambiguous[x]=matches
    report={"scope":"retained SDK dynamic symbol tables only; no OEM loader, MFi, or phone proof",
            "needed":needed,"imports":len(imports),"matched":len(resolved),"unresolved":unresolved,
            "multiple_providers":ambiguous,"installable":False}
    out.write_text(json.dumps(report,indent=2)+"\n")
    print("R105310_SDK_SYMBOL_CLOSURE",json.dumps(report))
    if unresolved:print("STATIC_UNRESOLVED_DO_NOT_REUSE_UNCHANGED")
    else:print("STATIC_STRONG_SYMBOLS_CLOSED_NOT_RUNTIME_PROOF")
if __name__=="__main__":main()
