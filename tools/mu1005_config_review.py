#!/usr/bin/env python3
"""Offline exact-byte configuration delta and original reader evidence. Never install."""
import argparse,hashlib,json,re
from pathlib import Path
from mu1005_config_schema import PREFIX,PINS
from mu1005_oem_inventory import inventory

def sha(data):return hashlib.sha256(data).hexdigest()
PAT=re.compile(rb'^([ \t]*"enableIap"[ \t]*:[ \t]*)false(?=[ \t]*(?:,|\r?$))',re.M)
def delta(data):
    matches=list(PAT.finditer(data))
    if len(matches)!=1 or data.count(b"enableIap")!=1:
        raise ValueError("not one exact standalone false setting")
    m=matches[0];start=m.end()-5
    new=data[:start]+b"true"+data[start+5:]
    restored=new[:start]+b"false"+new[start+4:]
    if restored!=data:raise ValueError("rollback mismatch")
    return new,{"offset":start,"line":data[:start].count(b"\n")+1,"original_sha256":sha(data),"candidate_sha256":sha(new),"restored_sha256":sha(restored),"changed_fields":1,"byte_exact_rollback":True}

def paths(root):
    for f in sorted(root.rglob("*")):
        if f.is_symlink() or not f.is_file() or not f.resolve().is_relative_to(root):continue
        if "filesystems" not in f.relative_to(root).parts:continue
        if f.stat().st_size>32*1024*1024:continue
        yield f

def reader(root):
    tokens=(b"connectivity.json",b"bluetooth.enableIap",b"enableIap",b"production/connectivity")
    rows=[]
    for f in paths(root):
        with f.open("rb") as h:
            if h.read(4)!=b"\x7fELF":continue
        d=f.read_bytes()
        hits=[t.decode() for t in tokens if t in d]
        if hits:rows.append({"path":str(f.relative_to(root)),"sha256":sha(d),"literal_references":hits})
    return {"elf_literal_references":rows,"reference_only":True}

def aliases(root):
    names={"libc.so.3","libecpp-ne.so.4","libm.so.2","libsocket.so.3","libnvos.so","libnvrm.so"}
    rows=[]
    for f in paths(root):
        if not (f.name in names or any(f.name.startswith(n.split(".so")[0]+".so") for n in names)):continue
        try:
            d=f.read_bytes();info=inventory(f)
            rows.append({"path":str(f.relative_to(root)),"sha256":sha(d),"needed":info["needed"]})
        except (ValueError,OSError):continue
    return {"near_name_candidates":rows,"candidate_only":True}

def selftest():
    old=b'{\r\n# comment\r\n "enableIap": false,\r\n "other": 12\r\n}'
    n,r=delta(old)
    assert n==old.replace(b"false",b"true") and r["byte_exact_rollback"]
    for bad in [b'// "enableIap": false,',b'"enableIap": true',b'"enableIap": false,\n"enableIap": false,']:
        try:delta(bad)
        except ValueError:continue
        raise AssertionError("accepted invalid fixture")
    print("EXACT_DELTA_FIXTURES_PASS")

def main():
    p=argparse.ArgumentParser();p.add_argument("root",type=Path,nargs="?");p.add_argument("lane",nargs="?");p.add_argument("out",type=Path,nargs="?");p.add_argument("--self-test",action="store_true");a=p.parse_args()
    if a.self_test:selftest();return
    root=a.root.resolve(strict=True)
    if a.lane=="reversible-delta":
        key=PREFIX+"production/connectivity.json";f=(root/key).resolve(strict=True)
        if not f.is_relative_to(root):raise ValueError("outside root")
        data=f.read_bytes()
        if sha(data)!=PINS[key]:raise ValueError("pin mismatch")
        new,r=delta(data)
        assert r["line"]==216
        # In-memory candidate only, do not publish raw OEM config or write it back.
    elif a.lane=="app70-reader":r=reader(root)
    elif a.lane=="library-aliases":r=aliases(root)
    else:raise ValueError("unknown lane")
    r.update(runtime_verified=False,installable=False)
    with a.out.open("x") as f:json.dump(r,f,indent=2)
    print("R105102_REVIEW",a.lane,json.dumps(r))
if __name__=="__main__":main()
