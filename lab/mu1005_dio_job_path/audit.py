#!/usr/bin/env python3
"""Read-only DIO config key 10 / native job dispatch investigation, exact MU1005."""
import sys,io,struct,json,re
from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original,sha
root=Path(sys.argv[1]).resolve();data=get_original(root,"dio_manager")
e=ELFFile(io.BytesIO(data));text=e.get_section_by_name(".text")
base=int(text["sh_addr"]);code=text.data()
segs=[(int(s["p_vaddr"]),s.data()) for s in e.iter_segments() if s["p_type"]=="PT_LOAD"]
def at(a,n):
    for v,d in segs:
        if v<=a and a+n<=v+len(d):return d[a-v:a-v+n]
    return b""
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
def region(a,z):
    return [{"va":hex(i.address),"op":i.mnemonic,"args":i.op_str} for i in md.disasm(at(a,z),a)]
# Previous exact-image findings: config accessor, producer, queue submit,
# queue worker, listener lookup. Follow only these established functions.
targets={"config_accessor":0x130bac,"producer":0x124990,"queue_submit":0x1365c4,"queue_worker":0x1362fc,"listener_lookup":0x124ac0}
regions={k:region(a,0x110 if k!="queue_submit" else 0xc0) for k,a in targets.items()}
regions["manager_config_call"]=region(0x156c90,0x70)
for k,rows in regions.items():
    if not rows:raise ValueError("missing exact region "+k)
    print("DIO_JOB_REGION",k,json.dumps(rows))
calls={k:[] for k in targets}
for off in range(0,len(code)-3,4):
    w=struct.unpack_from("<I",code,off)[0]
    if w&0x0f000000!=0x0b000000 or w>>28==15:continue
    rel=(w&0xffffff)<<2
    if rel&0x2000000:rel-=0x4000000
    a=base+off;dst=(a+8+rel)&0xffffffff
    for k,v in targets.items():
        if v==dst:calls[k].append(hex(a))
# Only public device-node paths and config filenames, never credentials.
nodes=sorted({m.decode() for _,d in segs for m in re.findall(rb"/dev/[A-Za-z0-9_./-]{2,120}",d) if re.search(rb"iap|ipod",m,re.I)})
cfg=[]
for p in root.rglob("*"):
    if p.is_symlink() or not p.is_file() or p.suffix.lower() not in (".json",".cfg",".conf",".ini",".xml"):continue
    if p.stat().st_size>2*1024*1024:continue
    try:raw=p.read_bytes()
    except OSError:continue
    found=sorted({m.decode() for m in re.findall(rb"/dev/[A-Za-z0-9_./-]{2,120}",raw) if re.search(rb"iap|ipod",m,re.I)})
    if found:cfg.append({"file":str(p.relative_to(root)),"device_paths":found})
print("DIO_JOB_CALLERS",json.dumps(calls))
print("DIO_IAP2_PATH_CANDIDATES",json.dumps({"binary":nodes,"configuration":cfg}))
Path(sys.argv[2]).write_text(json.dumps({"sha256":sha(data),"regions":regions,"callers":calls,"binary_paths":nodes,"configuration_paths":cfg,"runtime_verified":False,"installable":False},indent=2)+"\n")
