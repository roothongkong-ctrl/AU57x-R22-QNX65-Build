#!/usr/bin/env python3
"""Exact MU1005 peer selection and client RFCOMM call contracts, static only."""
import hashlib,json,struct,sys
from pathlib import Path
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
stock,out=map(Path,sys.argv[1:])
d=stock.read_bytes()
assert hashlib.sha256(d).hexdigest()=="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
ph=struct.unpack_from("<I",d,28)[0]
size,count=struct.unpack_from("<HH",d,42)
segments=[(v,v+n,o,flags) for k,o,v,_,n,_,flags,_ in (struct.unpack_from("<8I",d,ph+i*size) for i in range(count)) if k==1]
def read(a,n):
    for lo,hi,o,flags in segments:
        if lo<=a and a+n<=hi:return d[o+a-lo:o+a-lo+n]
    raise ValueError(hex(a))
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
md.detail=True
def instructions(a,n):
    rows=[]
    for va in range(a,a+n,4):
        i=next(md.disasm(read(va,4),va,count=1),None)
        if i is None:continue
        row={"va":hex(va),"op":i.mnemonic,"args":i.op_str}
        if i.mnemonic.startswith("bl") and i.operands and i.operands[0].type==ARM_OP_IMM:
            row["call"]=hex(i.operands[0].imm)
        rows.append(row)
    return rows
def branch(w,a):
    if w>>28==15 or w&0x0f000000!=0x0b000000:return None
    n=w&0xffffff
    if n&0x800000:n-=0x1000000
    return a+8+4*n
targets={"remote_device":0x242670,"client_rfcomm":0x2774c4,"sdp_query":0x1d0c08,"sdp_parse":0x1d0210}
hits={name:[] for name in targets}
for lo,hi,o,flags in segments:
    if not flags&1:continue
    for off in range(0,hi-lo-3,4):
        a=lo+off;v=branch(struct.unpack_from("<I",d,o+off)[0],a)
        for name,target in targets.items():
            if v==target:
                hits[name].append({"site":hex(a),"window":instructions(max(lo,a-64),min(100,hi-max(lo,a-64)))})
# Distinguish the original ACL job from a general remote device lookup.
# Inspect previous witnesses plus exact function beginnings, preserving return
# register uses and arguments at each original caller, rather than just offsets.
report={"scope":"STATIC ORIGINAL CALLER ARGUMENT AND RETURN EVIDENCE ONLY",
 "target_entries":{k:{"address":hex(a),"entry":instructions(a,96)} for k,a in targets.items()},
 "calls":hits,"acl_job_context":instructions(0x13f6c8,96),
 "paired_snapshot_provider_verified":False,"client_open_abi_verified":False,
 "installable":False,"runtime_verified":False}
for k,v in hits.items():print("EXACT_CALLERS",k,len(v),json.dumps(v[:8]),flush=True)
print("ACL_JOB_AFTER_TRACE",json.dumps(report["acl_job_context"]))
out.write_text(json.dumps(report,indent=2)+"\n")
assert hits["remote_device"] and hits["client_rfcomm"],"Prior original call candidates need review"
