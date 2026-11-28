#!/usr/bin/env python3
"""Read previously excluded same-variant original MMX images, no OEM export."""
import sys
# This entry point is named inspect.py; exclude its own directory before
# any imports can resolve Python's standard-library inspect to this script.
if sys.path:
    sys.path.pop(0)
import ctypes,ctypes.util,hashlib,json,subprocess
from pathlib import Path,PurePosixPath
archive,toolkit,out=map(Path,sys.argv[1:])
sys.path.insert(0,str(toolkit.resolve()/"tools"))
from extract_firmware import list_members,package_prefix
from imagefs import inventory_imagefs
from mifs_stage2 import decompress_container
names={"libnvdc.so","libnvos.so","libnvrm.so","libnvrm_graphics.so"}
def match(n):return n in names or any(n.startswith(x+".") for x in names)
members=list_members(Path("/usr/bin/7z"),archive);prefix=package_prefix(members,None);base=prefix+"/" if prefix else ""
lib=ctypes.CDLL(ctypes.util.find_library("lzo2") or "liblzo2.so.2")
fn=lib.lzo1z_decompress_safe
fn.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.POINTER(ctypes.c_size_t),ctypes.c_void_p]
fn.restype=ctypes.c_int
def decompress(data,limit):
    a=ctypes.create_string_buffer(data);b=ctypes.create_string_buffer(limit);n=ctypes.c_size_t(limit)
    if fn(a,len(data),b,ctypes.byref(n),None) or not 0<n.value<=limit:raise ValueError("LZO decode failed")
    return b.raw[:n.value]
out.mkdir(parents=True,exist_ok=True)
work=out.parent/"private-extra";work.mkdir(exist_ok=False)
rows=[];loose=[]
for i,item in enumerate(members):
    if not item["path"].startswith(base):continue
    p=PurePosixPath(item["path"][len(base):]);parts=p.parts
    if not parts or parts[0]!="MMX2":continue
    if match(p.name):loose.append({"path":str(p),"bytes":item["size"]})
    if p.suffix!=".img":continue
    same=len(parts)==5 and parts[2]=="70" and parts[3]=="default"
    prior=same and (parts[1] in ("app","mifs-stage2") or parts[1].startswith("efs-"))
    row={"path":str(p),"bytes":item["size"],"same_variant":same,"previously_extracted":prior};rows.append(row)
    if not same or prior or not 88<=item["size"]<=512*1024*1024:continue
    raw=work/f"{i}.img"
    with raw.open("xb") as f:subprocess.run(["7z","x","-so","-spd",str(archive.resolve()),"--",item["path"]],stdout=f,check=True,timeout=1200)
    if raw.stat().st_size!=item["size"]:raise ValueError("size mismatch")
    with raw.open("rb") as f:magic=f.read(7)
    try:
        decoded=raw
        if magic.startswith(b"LZOZ"):
            decoded=work/f"{i}.imagefs"
            decompress_container(raw,decoded,decompress=decompress,max_output_bytes=512*1024*1024)
        elif magic!=b"imagefs":
            # Stage1 may contain a boot header before the ImageFS payload.
            # Search bounded raw bytes, validate header and directory before
            # accepting any embedded candidate; never treat strings as files.
            import struct
            payload=raw.read_bytes()
            candidates=[]; hits=[]; pos=0
            while True:
                pos=payload.find(b"imagefs",pos)
                if pos<0:break
                off=pos;pos+=7
                if len(hits)>=256:raise ValueError("too many ImageFS signature candidates")
                hits.append(off)
                if off+88>len(payload):continue
                order=">" if payload[off+7]&1 else "<"
                size,end,start=struct.unpack_from(order+"III",payload,off+8)
                if not 88<=start<=end<=size-4 or size>len(payload)-off:continue
                candidate=work/f"{i}-embedded-{off}.imagefs"
                candidate.write_bytes(payload[off:off+size])
                try:inventory_imagefs(candidate)
                except (ValueError,OSError):candidate.unlink();continue
                candidates.append((off,candidate))
            row["imagefs_signature_offsets"]=hits
            row["validated_embedded_offsets"]=[off for off,_ in candidates]
            if len(candidates)!=1:
                row["result"]="no unique validated embedded ImageFS";continue
            row["embedded_imagefs_offset"],decoded=candidates[0]
        info=inventory_imagefs(decoded);row["result"]="validated ImageFS directory";row["matches"]=[]
        with decoded.open("rb") as f:
            for e in info["entries"]:
                if not match(PurePosixPath(e["path"]).name):continue
                m={k:e[k] for k in ("path","type")};row["matches"].append(m)
                if e["type"]=="symlink":m["target"]=e["target"]
                elif e["type"]=="file":
                    f.seek(e["data_offset"]);data=f.read(e["bytes"])
                    if len(data)!=e["bytes"]:raise ValueError("truncated extent")
                    m.update(bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),arm_elf32_le=len(data)>=20 and data[:6]==b"\x7fELF\x01\x01" and int.from_bytes(data[18:20],"little")==40)
    except (ValueError,OSError) as e:row["result"]="unsupported/invalid: "+str(e)[:180]
report={"scope":"same variant extra images; other variants listed, not mixed","images":rows,"loose_matches":loose,"installable":False}
(out/"coverage.json").write_text(json.dumps(report,indent=2)+"\n");print(json.dumps(report,indent=2))
