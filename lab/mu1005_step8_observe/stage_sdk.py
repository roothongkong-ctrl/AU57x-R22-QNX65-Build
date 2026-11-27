#!/usr/bin/env python3
"""Stage one pinned QNX SDP runtime only for isolated BT guests."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
name="libecpp-ne.so.4"
pin="a01457f59746ed4914108046dd5bf5a308989c99"
sdk=Path(sys.argv[1]).resolve(strict=True)
stage=Path(sys.argv[2]).resolve(strict=True)
report=Path(sys.argv[3])
original=json.loads(Path(sys.argv[4]).read_text())
src=sdk/"sdp/target/qnx6/armle-v7/lib"/name
def cmd(*args):
    return subprocess.check_output(args,text=True,timeout=30).strip()
def require(condition,message):
    if not condition:raise ValueError(message)
require(len(sys.argv)==6 and sys.argv[5] in ("btstack","bluetooth","connectivity_launcher") and original["program"]==sys.argv[5],"BT experiment only")
require(name in original["missing"] and name not in original["ambiguous"],"expected missing dependency changed")
require(not (stage/name).exists(),"never overwrite staged original")
require(src.is_file() and not src.is_symlink(),"SDK regular file required")
require(src.resolve().is_relative_to(sdk),"SDK containment")
require(cmd("git","-C",str(sdk),"hash-object",str(src))==pin,"SDK blob mismatch")
header=src.read_bytes()[:20]
require(header[:6]==b"\x7fELF\x01\x01" and header[16:20]==b"\x03\x00\x28\x00","not ARM32 shared ELF")
dyn=cmd("readelf","-dW",str(src))
soname=re.findall(r"\(SONAME\).*?\[([^\]]+)\]",dyn)
require(soname==[name],"SDK SONAME mismatch")
deps=sorted(set(re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",dyn)))
allowed=set(original["sdk_baseline"])|set(original["selected"])
missing=sorted(set(deps)-allowed)
result={"scope":"QNX_SDP_LAB_ONLY_NOT_OEM_NOT_INSTALL","name":name,"git_blob":pin,
        "sha256":hashlib.sha256(src.read_bytes()).hexdigest(),"needed":deps,
        "unresolved_direct_needed":missing,"staged":False,"runtime_verified":False}
report.write_text(json.dumps(result,indent=2)+"\n")
require(not missing,"SDK candidate introduces unresolved dependencies")
shutil.copyfile(src,stage/name)
result["staged"]=True
report.write_text(json.dumps(result,indent=2)+"\n")
print("BT_PINNED_SDK_RUNTIME_STAGED",name,result["sha256"])
