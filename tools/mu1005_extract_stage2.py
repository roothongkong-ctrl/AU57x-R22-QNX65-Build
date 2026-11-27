#!/usr/bin/env python3
"""Extract MMX variant 70 stage2/EFS from a retained archive, read-only."""
import argparse,ctypes,ctypes.util,json,subprocess,sys
from pathlib import Path,PurePosixPath
def main():
    p=argparse.ArgumentParser()
    p.add_argument("archive",type=Path);p.add_argument("root",type=Path)
    p.add_argument("--toolkit",type=Path,required=True);p.add_argument("--qnxmount",type=Path,required=True)
    p.add_argument("--report",type=Path,required=True)
    a=p.parse_args()
    sys.path[:0]=[str(a.toolkit.resolve()/"tools"),str(a.qnxmount.resolve())]
    from extract_firmware import list_members,extract_rcc_efs
    from mifs_stage2 import decompress_container
    from imagefs import materialize_imagefs
    import hashlib
    lib=ctypes.CDLL(ctypes.util.find_library("lzo2") or "liblzo2.so.2")
    fn=lib.lzo1z_decompress_safe
    fn.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.POINTER(ctypes.c_size_t),ctypes.c_void_p]
    fn.restype=ctypes.c_int
    def decompress(data,limit):
        src=ctypes.create_string_buffer(data)
        dest=ctypes.create_string_buffer(limit)
        n=ctypes.c_size_t(limit)
        rc=fn(src,len(data),dest,ctypes.byref(n),None)
        if rc or not 0<n.value<=limit:raise ValueError(f"LZO1Z decode failed {rc}")
        return dest.raw[:n.value]
    entries=list_members(Path("/usr/bin/7z"),a.archive.resolve())
    selected=[]
    for x in entries:
        parts=PurePosixPath(x["path"]).parts
        for i in range(len(parts)-4):
            mm=parts[i:]
            if len(mm)==5 and mm[0]=="MMX2" and mm[2]=="70" and mm[3]=="default":
                if (mm[1]=="mifs-stage2" and mm[4]=="mifs-stage2.img") or (mm[1].startswith("efs-") and mm[4].endswith(".img")):
                    selected.append((x,Path(*mm)));break
    stage=[x for x in selected if x[1].parts[1]=="mifs-stage2"]
    if len(stage)!=1:raise ValueError(f"expected exactly one stage2, found {len(stage)}")
    if len({str(rel) for _,rel in selected})!=len(selected):raise ValueError("duplicate output paths")
    a.root.mkdir(parents=True,exist_ok=False)
    raw=a.root/"raw";raw.mkdir()
    (a.root/"metadata").mkdir();(a.root/"decoded").mkdir();(a.root/"filesystems").mkdir()
    reports=[]
    for x,rel in selected:
        dest=raw/rel;dest.parent.mkdir(parents=True,exist_ok=True)
        with dest.open("xb") as out:
            subprocess.run(["7z","x","-so","-spd",str(a.archive.resolve()),"--",x["path"]],stdout=out,check=True,timeout=1200)
        if dest.stat().st_size!=x["size"]:raise ValueError("extracted length mismatch")
        if rel.parts[1]=="mifs-stage2":
            decoded=a.root/"decoded"/"stage2.imagefs"
            info=decompress_container(dest,decoded,decompress=decompress)
            digest=hashlib.sha256(decoded.read_bytes()).hexdigest()
            tree=a.root/"filesystems"/"stage2"
            materialize_imagefs(decoded,tree,a.root/"metadata"/"stage2.json")
            reports.append({"member":str(rel),"decoded_sha256":digest,"decode":info})
        else:
            reports.append({"member":str(rel),"efs":extract_rcc_efs(dest,a.root)})
    a.report.write_text(json.dumps(reports,indent=2)+"\n")
    print("OEM_STAGE2_EFS_EXTRACTED",len(reports))
if __name__=="__main__":main()
