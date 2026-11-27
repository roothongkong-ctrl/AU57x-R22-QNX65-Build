#!/usr/bin/env python3
"""Inspect exact OEM config syntax and enableIap fields; no writes to inputs."""
import argparse, hashlib, json, re
from pathlib import Path
from r99_stock_iap2_route import no_duplicates

PREFIX="stage2/filesystems/MMX2/efs-sys/70/default/efs-system.img/etc/eso/"
PINS={
    PREFIX+"presets/bluetooth.json":"f2ed3ee91b6a02a3405cbaa273db0816a03f8f2fdf98ba2814e6e37b5b238250",
    PREFIX+"production/connectivity.json":"56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470",
}
TOKEN=re.compile(r'"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/|,|\s+|.',re.S)
def relaxed(text):
    tokens=TOKEN.findall(text)
    tokens=[t if not t.startswith(("//","/*")) else " " for t in tokens]
    out=[]
    for i,t in enumerate(tokens):
        if t==",":
            next_token=next((v for v in tokens[i+1:] if not v.isspace()),"")
            if next_token in ("}","]"):continue
        out.append(t)
    return "".join(out)

def inspect(data):
    text=data.decode("utf-8-sig")
    result={"syntax":"strict_json","fields":[]}
    try:
        obj=json.loads(text,object_pairs_hook=no_duplicates)
    except ValueError:
        try:
            obj=json.loads(relaxed(text),object_pairs_hook=no_duplicates)
            result["syntax"]="json_with_comments_or_trailing_commas"
        except ValueError as exc:
            result["syntax"]="unrecognized"
            result["parse_error_type"]=type(exc).__name__
            if isinstance(exc,json.JSONDecodeError):
                result.update(error_line=exc.lineno,error_column=exc.colno)
            result["enableIap_literal_occurrences"]=len(re.findall(r"enableIap",text))
            return result
    def walk(v,path):
        if isinstance(v,dict):
            for k,val in v.items():
                new=path+[k]
                if "enableiap" in k.lower():
                    result["fields"].append({"path":new,"type":type(val).__name__,"boolean":val if type(val) is bool else None})
                walk(val,new)
        elif isinstance(v,list):
            for i,item in enumerate(v):walk(item,path+[i])
    walk(obj,[])
    result["root_type"]=type(obj).__name__
    result["root_keys"]=sorted(obj.keys()) if isinstance(obj,dict) else []
    return result

def self_test():
    assert inspect(b'{"bluetooth":{"enableIap":false}}')["fields"]==[{"path":["bluetooth","enableIap"],"type":"bool","boolean":False}]
    a=inspect(b'{"x":"http://a/*b*/", // comment\n "items":[{"enableIap":true,},],}')
    assert a["syntax"]=="json_with_comments_or_trailing_commas" and a["fields"][0]["path"]==["items",0,"enableIap"]
    assert inspect(b'{"enableIap":false,"enableIap":true}')["syntax"]=="unrecognized"
    assert inspect(b'{"enableIap":"false"}')["fields"][0]["boolean"] is None
    print("CONFIG_SCHEMA_FIXTURES_PASS")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("root",type=Path,nargs="?");p.add_argument("out",type=Path,nargs="?");p.add_argument("--self-test",action="store_true")
    a=p.parse_args()
    if a.self_test:self_test();return
    if not a.root or not a.out:p.error("root and out required")
    root=a.root.resolve(strict=True);rows=[]
    for path,digest in PINS.items():
        f=(root/path).resolve(strict=True)
        if not f.is_relative_to(root) or f.stat().st_size>1024*1024:raise ValueError("invalid config candidate")
        data=f.read_bytes()
        if hashlib.sha256(data).hexdigest()!=digest:raise ValueError("original hash mismatch")
        r=inspect(data);r.update(path=path,sha256=digest);rows.append(r)
        print("OEM_CONFIG_SCHEMA",json.dumps(r))
    with a.out.open("x") as f:json.dump({"candidates":rows,"runtime_verified":False,"installable":False},f,indent=2)
if __name__=="__main__":main()
