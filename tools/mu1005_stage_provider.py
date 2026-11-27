#!/usr/bin/env python3
"""Private offline config staging and real ELF symbol provider search. No install."""
import hashlib,json,os,re,subprocess,sys,tempfile
from pathlib import Path
from mu1005_config_review import delta
from mu1005_config_schema import PREFIX,PINS

def sha(b):return hashlib.sha256(b).hexdigest()
def write_private(folder,name,data):
    with (folder/name).open("xb") as f:
        os.fchmod(f.fileno(),0o600);f.write(data);f.flush();os.fsync(f.fileno())

def stage(original,folder):
    candidate,report=delta(original)
    write_private(folder,"original.review",original)
    write_private(folder,"candidate.review",candidate)
    return report

def verify(folder,original_sha):
    original=(folder/"original.review").read_bytes()
    candidate=(folder/"candidate.review").read_bytes()
    assert sha(original)==original_sha,"original mismatch"
    expected,report=delta(original)
    assert candidate==expected,"candidate mismatch"
    offset=report["offset"]
    restored=candidate[:offset]+b"false"+candidate[offset+4:]
    assert restored==original,"rollback mismatch"
    return report

def private_stage(root):
    key=PREFIX+"production/connectivity.json"
    path=(root/key).resolve(strict=True)
    assert path.is_relative_to(root) and path.is_file()
    original=path.read_bytes()
    assert sha(original)==PINS[key]
    with tempfile.TemporaryDirectory(prefix="mu1005-private-") as temp:
        folder=Path(temp)
        stage(original,folder)
        report=verify(folder,PINS[key])
        assert report["line"]==216
        assert path.read_bytes()==original
    report.update(private_stage_verified=True,source_unchanged=True,raw_files_published=False)
    return report

def scan(root):
    found=[];errors=[];candidates=0
    for p in sorted(root.rglob("*")):
        if p.is_symlink() or not p.is_file() or not p.resolve().is_relative_to(root):continue
        if p.stat().st_size>32*1024*1024:continue
        with p.open("rb") as f:
            head=f.read(20)
            if head[:6]!=b"\x7fELF\x01\x01" or head[18:20]!=b"\x28\x00":continue
            data=head+f.read()
        if b"slogf\x00" not in data:continue
        candidates+=1
        try:
            s=subprocess.check_output(["readelf","-D","-sW",str(p)],text=True,stderr=subprocess.STDOUT,timeout=30)
            lines=[]
            for line in s.splitlines():
                a=line.split()
                if len(a)<8 or not re.fullmatch(r"\d+:",a[0]):continue
                if a[7].split("@",1)[0]!="slogf":continue
                lines.append({"symbol":a[7],"defined":a[6]!="UND","binding":a[4],"visibility":a[5]})
            exports=[x for x in lines if x["defined"] and x["binding"] in ("GLOBAL","WEAK") and x["visibility"] in ("DEFAULT","PROTECTED")]
            if lines:found.append({"path":str(p.relative_to(root)),"sha256":sha(data),"symbol_rows":lines,"export_candidate":bool(exports)})
        except (OSError,subprocess.SubprocessError) as e:errors.append({"path":str(p.relative_to(root)),"error":type(e).__name__})
    return {"literal_candidates":candidates,"symbol_results":found,"export_candidates":[x["path"] for x in found if x["export_candidate"]],"inspection_errors":errors,"scope":"static_symbol_provider_only"}

def self_test():
    old=b'{\r\n# comment\r\n "enableIap": false,\r\n "other": 12\r\n}'
    with tempfile.TemporaryDirectory() as t:
        p=Path(t)
        stage(old,p)
        assert verify(p,sha(old))["byte_exact_rollback"]
        (p/"candidate.review").write_bytes(b"corrupted")
        try:verify(p,sha(old))
        except AssertionError:pass
        else:raise AssertionError("tampered candidate accepted")
    print("PRIVATE_STAGE_FIXTURES_PASS")

def main():
    lane=sys.argv[1]
    if lane=="self-test":self_test();return
    root=Path(sys.argv[2]).resolve(strict=True);out=Path(sys.argv[3])
    r=private_stage(root) if lane=="private-stage" else scan(root)
    r.update(lane=lane,runtime_verified=False,installable=False)
    with out.open("x") as f:json.dump(r,f,indent=2)
    print("R105104_EVIDENCE",json.dumps(r))
if __name__=="__main__":main()
