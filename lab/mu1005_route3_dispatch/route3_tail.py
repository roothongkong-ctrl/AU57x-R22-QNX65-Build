#!/usr/bin/env python3
"""Route 3 continuation: inspect ONLY the previously uncovered raw branch and receive entry."""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2])
path=(root/"app70/filesystems/MMX2/app/70/default/app.img/armle/lib/dll/ipod-drvr-iap2.so").resolve(strict=True)
if not path.is_relative_to(root) or not path.is_file():
    raise ValueError("driver outside extracted archive")
data=path.read_bytes()
digest=hashlib.sha256(data).hexdigest()
if digest!="42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf":
    raise ValueError("driver identity mismatch")
b=Binary(data)
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
md.detail=True
def symbol_size(addr,name):
    sizes={n for a,n,s in b.funcs if a==addr and s==name and n>0}
    if len(sizes)!=1:
        raise ValueError("unresolved symbol size: "+name)
    return sizes.pop()
def decode(start,end):
    if not (b.va<=start<end<=b.va+len(b.code)) or end-start>8192 or start%4 or end%4:
        raise ValueError("invalid bounded ARM code range")
    # A literal pool or invalid word must never prevent decoding a known later
    # direct branch target. Preserve gaps and ELF ARM mapping markers.
    marks=sorted((addr,name.split(".")[0]) for addr,names in b.names.items()
                 for name in names if name.split(".")[0] in ("$a","$d","$t")
                 and b.va<=addr<b.va+len(b.code))
    rows=[]
    for addr in range(start,end,4):
        mode=next((mode for va,mode in reversed(marks) if va<=addr),None)
        row={"va":hex(addr),"mapping":mode}
        if mode in ("$d","$t"):
            row["gap"]="mapped data or Thumb, not decoded as ARM"
        else:
            word=b.code[addr-b.va:addr-b.va+4]
            i=next(md.disasm(word,addr,count=1),None)
            if i is None or i.size!=4:
                row["gap"]="undecodable ARM word"
            else:
                row["insn"]=i.mnemonic+" "+i.op_str
                if i.mnemonic in ("b","bl","blx","beq","bne","bhs","blo","bmi","bpl","bvs","bvc","bhi","bls","bge","blt","bgt","ble","bcs","bcc") and i.operands and i.operands[0].type==ARM_OP_IMM:
                    target=i.operands[0].imm
                    row.update(target=hex(target),labels=b.labels(target))
        rows.append(row)
    return rows
msg=0xf4c8
end=msg+symbol_size(msg,"iap2_msg")
# R105.177 stopped at 0x104a8; do not re-run its previous instruction window.
tail=0x104ac
if not tail<=0x10680<end:
    raise ValueError("raw branch outside original function symbol")
rows=decode(tail,end)
raw=decode(0x10680,min(end,0x106b4))
recv=0x2121c
recv_end=recv+symbol_size(recv,"transport_recv_pkt")
receive=decode(recv,recv_end)

res={"scope":"NEW_STATIC_TAIL_AND_RECEIVE_ENTRY_ONLY","driver_sha256":digest,
     "prior_artifact":15211206944,"iap2_msg_end":hex(end),"new_tail":rows,
     "raw_branch_3000":raw,"transport_recv_pkt":receive,"runtime_verified":False,
     "wireless_producer_verified":False}
out.write_text(json.dumps(res,indent=2)+"\n")
print("ROUTE3_RAW_BRANCH_3000",json.dumps(raw))
print("ROUTE3_RECV_ENTRY_CALLS",json.dumps([r for r in receive if r.get("labels")]))
if not raw or raw[0]["va"]!="0x10680" or "insn" not in raw[0]:
    raise ValueError("raw branch target not ARM code; mapping evidence retained")
if not receive or receive[0]["va"]!=hex(recv) or "insn" not in receive[0]:
    raise ValueError("receive entry not ARM code; mapping evidence retained")
print("ROUTE3_NEW_TAIL_AND_RECV_STATIC_ONLY_NOT_WIRELESS")
