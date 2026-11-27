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
    if not (b.va<=start<end<=b.va+len(b.code)) or end-start>8192:
        raise ValueError("invalid bounded code range")
    rows=[]
    for i in md.disasm(b.code[start-b.va:end-b.va],start):
        row={"va":hex(i.address),"insn":i.mnemonic+" "+i.op_str}
        if i.mnemonic.startswith("b") and i.operands and i.operands[0].type==ARM_OP_IMM:
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
raw=[r for r in rows if 0x10680<=int(r["va"],16)<min(end,0x106b4)]
if not raw or raw[0]["va"]!="0x10680":
    raise ValueError("raw branch not decoded")
recv=0x2121c
recv_end=recv+symbol_size(recv,"transport_recv_pkt")
receive=decode(recv,recv_end)
if not receive or receive[0]["va"]!=hex(recv):
    raise ValueError("receive entry not decoded")
res={"scope":"NEW_STATIC_TAIL_AND_RECEIVE_ENTRY_ONLY","driver_sha256":digest,
     "prior_artifact":15211206944,"iap2_msg_end":hex(end),"new_tail":rows,
     "raw_branch_3000":raw,"transport_recv_pkt":receive,"runtime_verified":False,
     "wireless_producer_verified":False}
out.write_text(json.dumps(res,indent=2)+"\n")
print("ROUTE3_RAW_BRANCH_3000",json.dumps(raw))
print("ROUTE3_RECV_ENTRY_CALLS",json.dumps([r for r in receive if r.get("labels")]))
print("ROUTE3_NEW_TAIL_AND_RECV_STATIC_ONLY_NOT_WIRELESS")
