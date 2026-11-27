#!/usr/bin/env python3
"""Resolve exact stock driver descriptor and message callback candidates, no runtime calls."""
import sys,hashlib,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
root=Path(sys.argv[1]).resolve(strict=True); out=Path(sys.argv[2])
pins={"mm-ipod":"4b87cd2d41ddcee2f38800fe8afe8b4b340e4f23e1d86c7e97af303522aa15d09","ipod-drvr-iap2.so":"42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf"}
def exact(n):
 for p in sorted(root.rglob(n)):
  if p.is_file() and p.resolve().is_relative_to(root):
   d=p.read_bytes()
   if hashlib.sha256(d).hexdigest()==pins[n]:return Binary(d)
 raise ValueError("missing exact "+n)
def ptr(b,a):
 w=b.word(a)
 if w is None:return []
 rs=b.reloc.get(a,[])
 if not rs:return [w] if w else []
 result=set()
 for r in rs:
  add=r["addend"] if r["addend"] is not None else w
  if r["type"]==23:result.add(add&0xffffffff) # R_ARM_RELATIVE
  elif r["type"]==2 and r["symbol_va"]:result.add((r["symbol_va"]+add)&0xffffffff)
  elif r["type"] in (21,22) and r["symbol_va"]:result.add(r["symbol_va"])
 return sorted(result)
md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
def dis(b,a,n):
 if not b.va<=a<b.va+len(b.code):return []
 end=min(a+n,b.va+len(b.code))
 rows=[]
 for i in md.disasm(b.code[a-b.va:end-b.va],a):
  row={"va":hex(i.address),"insn":i.mnemonic+" "+i.op_str}
  if i.mnemonic in ("bl","blx","b") and i.operands and i.operands[0].type==ARM_OP_IMM:
   row["target_labels"]=b.labels(i.operands[0].imm)
  if any(o.type==ARM_OP_IMM and o.imm in (0x9999,0x3000) for o in i.operands):
   row["opcode_immediate_candidate"]=True
  rows.append(row)
 return rows
mm=exact("mm-ipod"); dr=exact("ipod-drvr-iap2.so")
# The existing audit identified the consumers; inspect the missing producer:
# exact driver module descriptor relocation and its device interface +0x24.
mods=sorted(v for v,ns in dr.names.items() if "ipod_module" in ns)
assert len(mods)==1,"ambiguous descriptor"
m=mods[0]
tables=ptr(dr,m+0x20)
res={"scope":"EXACT_ORIGINAL_DRIVER_DESCRIPTOR_CALLBACK_CANDIDATES_ONLY","sha256":pins,"module":hex(m),"module_abi":dr.word(m+4),"descriptor_relocations":dr.reloc.get(m+0x20,[]),"interface_candidates":[],"mm_ipod_driver_init":dis(mm,0x1035f4,200),"mm_ipod_iface_find":dis(mm,0x1033a4,364),"runtime_verified":False}
for t in tables:
 slots=[]
 for off in (0,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x3c,0x40,0x44):
  a=t+off
  slots.append({"offset":hex(off),"word":dr.word(a),"relocations":dr.reloc.get(a,[]),"targets":[{"va":hex(v),"labels":dr.labels(v),"owner":dr.owner(v&~1)} for v in ptr(dr,a)]})
 candidates=[]
 for v in ptr(dr,t+0x24):
  # Do not decode Thumb pointers as ARM or invent function lengths.
  o=dr.owner(v&~1)
  n=max((n for a,n,s in dr.funcs if a==v),default=0)
  if not v&1 and n and dr.va<=v<dr.va+len(dr.code):
   candidates.append({"va":hex(v),"owner":o,"disassembly":dis(dr,v,min(n,4096))})
 res["interface_candidates"].append({"va":hex(t),"labels":dr.labels(t),"slots":slots,"slot24_code":candidates})
out.write_text(json.dumps(res,indent=2)+"\n")
print("ROUTE3_DRIVER_DESCRIPTOR",hex(m),"INTERFACE_CANDIDATES",[hex(t) for t in tables])
for t in res["interface_candidates"]:
 print("ROUTE3_SLOT24",json.dumps(next(s for s in t["slots"] if s["offset"]=="0x24"),sort_keys=True))
 for c in t["slot24_code"]:
  print("ROUTE3_SLOT24_IMMEDIATE_CANDIDATES",json.dumps([i for i in c["disassembly"] if i.get("opcode_immediate_candidate")]))
assert tables,"driver descriptor target unresolved"
print("ROUTE3_EXACT_DESCRIPTOR_INSPECTED_NOT_RUNTIME")
