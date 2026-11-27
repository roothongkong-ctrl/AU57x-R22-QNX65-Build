#!/usr/bin/env python3
"""Classify original configuration syntax and locate launch references; no firmware writes."""
import argparse, hashlib, json, re
from pathlib import Path
from mu1005_config_schema import PREFIX, PINS

KEY=re.compile(r'(?<![\w.])(?:"(?:bluetooth\.)?enableIap"|(?:bluetooth\.)?enableIap)(?![\w.])\s*([:=])\s*(true|false|0|1)\b')
def describe(data):
    text=data.decode("utf-8-sig")
    lines=text.splitlines()
    hits=[]
    for i,line in enumerate(lines,1):
        if "enableIap" not in line:continue
        m=KEY.search(line)
        hits.append({"line":i,"key_style":"dotted" if "bluetooth.enableIap" in line else "local",
                     "separator":m[1] if m else None,"literal":m[2] if m else None,
                     "comment_prefix":line.lstrip().startswith(("#","//","/*","*",";")),
                     "match_count":len(KEY.findall(line))})
    # Emit only syntactic classes of the first lines, never configuration values.
    def shape(line):
        t=line.lstrip()
        for prefix in ("#include","#if","#else","#endif","//","/*","*","#",";","{","}","[","]",'"'):
            if t.startswith(prefix):return prefix
        return "empty" if not t else "other"
    return {"first_12_line_shapes":[shape(l) for l in lines[:12]],"enableIap_occurrences":hits,
            "raw_occurrence_count":text.count("enableIap"),"runtime_verified":False,"installable":False}

def references(root):
    needles=(b"connectivity.json",b"bluetooth.enableIap",b"presets/bluetooth.json",b"production/connectivity")
    rows=[]
    for f in sorted((root/"stage2/filesystems").rglob("*")):
        if f.is_symlink() or not f.is_file() or not f.resolve().is_relative_to(root):continue
        if f.stat().st_size>16*1024*1024:continue
        data=f.read_bytes()
        names=[n.decode() for n in needles if n in data]
        if names:rows.append({"path":str(f.relative_to(root)),"sha256":hashlib.sha256(data).hexdigest(),"references":names,
                              "elf":data.startswith(b"\x7fELF"),"reference_only":True})
    return {"references":rows,"runtime_verified":False,"installable":False}

def self_test():
    d=describe(b'{\n#if X\n"bluetooth.enableIap": false,\n#endif\n}')
    assert d["first_12_line_shapes"][1]=="#if"
    assert d["enableIap_occurrences"][0]["literal"]=="false"
    assert describe(b'// "enableIap": true')["enableIap_occurrences"][0]["comment_prefix"]
    assert describe(b'"enableIap":"true"')["enableIap_occurrences"][0]["literal"] is None
    print("CONNECTIVITY_LEXICAL_FIXTURES_PASS")

def main():
    p=argparse.ArgumentParser();p.add_argument("root",type=Path,nargs="?");p.add_argument("lane",choices=("syntax","launch-references"),nargs="?");p.add_argument("out",type=Path,nargs="?");p.add_argument("--self-test",action="store_true");a=p.parse_args()
    if a.self_test:self_test();return
    if not a.root or not a.out or not a.lane:p.error("root lane out required")
    root=a.root.resolve(strict=True)
    if a.lane=="syntax":
        key=PREFIX+"production/connectivity.json"
        f=(root/key).resolve(strict=True)
        if not f.is_relative_to(root) or f.stat().st_size>1024*1024:raise ValueError("invalid input")
        data=f.read_bytes()
        if hashlib.sha256(data).hexdigest()!=PINS[key]:raise ValueError("pin mismatch")
        report=describe(data)
    else:report=references(root)
    with a.out.open("x") as f:json.dump(report,f,indent=2)
    print("CONNECTIVITY_EVIDENCE",a.lane,json.dumps(report))
if __name__=="__main__":main()
