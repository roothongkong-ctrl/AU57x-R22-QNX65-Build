#!/usr/bin/env python3
"""Private offline MU1005 OEM input locator and ELF symbol inventory. No installation."""
import argparse, hashlib, json, os, re, struct
from pathlib import Path

PIN = {
    "btstack": "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566",
    "smartphone_integrator": "2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b",
    "dio_manager": "e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c",
    "libairplay.so": "193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5",
}
AUX = {"libiap2client.so.1", "libirc_mmx_adapter.so", "iap2.cfg"}
PAT = re.compile(r"mfi|authenticat|iap2|airplay|carplay|pairing|session|register|delegate|owner|activat", re.I)

def sha(path):
    with path.open("rb") as f:
        h = hashlib.sha256()
        for block in iter(lambda:f.read(1024*1024), b""):
            h.update(block)
        return h.hexdigest()

def inventory(path):
    d = path.read_bytes()
    if len(d)<52 or d[:6] != b"\x7fELF\x01\x01":
        raise ValueError("not ELF32 little-endian")
    h=struct.unpack_from("<16sHHIIIIIHHHHHH",d)
    if h[2]!=40 or h[11]!=40 or not h[12]:
        raise ValueError("not ARM ELF32 with section table")
    def block(off,n):
        if off<0 or n<0 or off+n>len(d): raise ValueError("truncated ELF")
        return d[off:off+n]
    raw=block(h[6],h[12]*40)
    secs=[struct.unpack_from("<10I",raw,i*40) for i in range(h[12])]
    def st(sec):
        if sec[6]>=len(secs): raise ValueError("invalid string table link")
        x=secs[sec[6]]
        return block(x[4],x[5])
    def name(tab,i):
        if i>=len(tab): raise ValueError("invalid string offset")
        j=tab.find(b"\0",i)
        if j<0: raise ValueError("unterminated string")
        return tab[i:j].decode("utf-8","replace")
    needed=set(); symbols=set(); tables=[]
    for sec in secs:
        if sec[1] in (2,11):
            if sec[9]!=16 or sec[5]%16: raise ValueError("bad symbol table")
            tab=st(sec); b=block(sec[4],sec[5]);tables.append("dynsym" if sec[1]==11 else "symtab")
            for i in range(0,len(b),16):
                n,va,size,info,other,idx=struct.unpack_from("<IIIBBH",b,i)
                v=name(tab,n)
                if PAT.search(v):
                    symbols.add((v,va,size,idx==0))
        elif sec[1]==6:
            if sec[9]!=8 or sec[5]%8: raise ValueError("bad dynamic section")
            tab=st(sec);b=block(sec[4],sec[5])
            for i in range(0,len(b),8):
                tag,val=struct.unpack_from("<iI",b,i)
                if tag==1: needed.add(name(tab,val))
    return {"elf_type":h[1],"machine":"ARM","needed":sorted(needed),
            "symbol_tables":sorted(set(tables)),
            "candidate_symbols":[{"name":n,"va":hex(v),"size":s,"imported":u} for n,v,s,u in sorted(symbols)],
            "candidate_only":True}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("root",type=Path,help="already extracted app.img/firmware directory; no raw image parsing")
    p.add_argument("--out",type=Path,required=True)
    a=p.parse_args()
    if not a.root.is_dir() or a.out.exists():p.error("root must be directory; output must not exist")
    root=a.root.resolve(); found={k:[] for k in sorted(set(PIN)|AUX)}
    errors=[]; paths={}
    for folder,dirs,files in os.walk(root,followlinks=False):
        dirs.sort()
        for n in sorted(files):
            paths.setdefault(n,[]).append(Path(folder)/n)
        for n in sorted(set(files)&set(found)):
            f=Path(folder)/n
            if f.is_symlink():continue
            try:
                entry={"relative_path":str(f.relative_to(root)),"bytes":f.stat().st_size,"sha256":sha(f)}
                if n in PIN:entry["matches_pinned"]=entry["sha256"]==PIN[n]
                if n!="iap2.cfg" and (n not in PIN or entry["matches_pinned"]):
                    try:entry["elf"]=inventory(f)
                    except (ValueError,struct.error) as e:entry["elf_error"]=str(e)
                found[n].append(entry)
            except OSError as e:errors.append({"relative_path":str(f.relative_to(root)),"error":str(e)})
    missing=[n for n in PIN if not any(x.get("matches_pinned") for x in found[n])]
    # Resolve the full DT_NEEDED closure without running an OEM executable.
    # Preserve candidates rather than guessing QNX LD_LIBRARY_PATH precedence.
    # Refuse symlinks outside the extracted tree. Scan each resolved file once.
    queue=sorted({dep for entries in found.values() for x in entries
                  for dep in x.get("elf",{}).get("needed",[])})
    deps={}; cache={}
    while queue:
        n=queue.pop(0)
        if n in deps:continue
        choices=[]
        for p in paths.get(n,[]):
            try:
                target=p.resolve(strict=True)
                if not target.is_relative_to(root) or not target.is_file():continue
                if target not in cache:
                    cache[target]={"sha256":sha(target),"elf":inventory(target)}
                x=cache[target]
                choices.append({"relative_path":str(p.relative_to(root)),
                                "resolved_path":str(target.relative_to(root)),
                                "sha256":x["sha256"],"needed":x["elf"]["needed"]})
                queue.extend(dep for dep in x["elf"]["needed"] if dep not in deps)
            except (OSError,RuntimeError,ValueError,struct.error) as e:
                errors.append({"relative_path":str(p.relative_to(root)),"error":str(e)})
        deps[n]=choices
    missing_deps=sorted(n for n,v in deps.items() if not v)
    ambiguous_deps=sorted(n for n,v in deps.items() if len({x["sha256"] for x in v})>1)
    result={"scope":"PRIVATE_OFFLINE_EVIDENCE_ONLY",
            "dependency_candidates":deps,"unresolved_dependencies":missing_deps,
            "ambiguous_dependencies":ambiguous_deps,"pinned_set_found":not missing,
            "missing_or_mismatched":missing,"files":found,"errors":errors,
            "runtime_verified":False,"mfi_verified":False,"installable":False}
    with a.out.open("x",encoding="utf-8") as f:json.dump(result,f,indent=2,ensure_ascii=False);f.write("\n")
    print("Matched pinned files:",len(PIN)-len(missing),"/",len(PIN))
    print("Missing/mismatched:",", ".join(missing) or "none")
    print("Dependency names:",len(deps),"unresolved:",len(missing_deps),"ambiguous:",len(ambiguous_deps))
    print("Offline evidence only; no firmware changed.")
    return 2 if missing else 0
if __name__=="__main__":raise SystemExit(main())
