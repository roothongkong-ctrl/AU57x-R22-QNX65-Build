#!/usr/bin/env python3
"""R105.158: bounded follow-up of exact OEM launcher callsite, no OEM execution."""
import hashlib,json,struct,sys
from pathlib import Path
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM,ARM_OP_MEM,ARM_REG_PC
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary,prel_branch
PIN="08896149dcdcc2bc5a6b22ce78682ad5949fbeb8fdc0fb1249162861e40914b8"
SETTER=0x10e310
NAME=0x136402

def original(root):
 for p in sorted(root.rglob("connectivity_launcher")):
  if p.is_file() and p.resolve().is_relative_to(root):
   d=p.read_bytes()
   if hashlib.sha256(d).hexdigest()==PIN:return Binary(d)
 raise ValueError("pinned launcher missing")

def mapped_words(b,value):
 pattern=struct.pack("<I",value); out=[]
 for off,va,n in b.loads:
  data=b.d[off:off+n]; start=0
  while (pos:=data.find(pattern,start))>=0:
   start=pos+1
   out.append(va+pos)
 return sorted(set(out))

def windows(b,lo,hi):
 md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
 lo=max(lo,b.va);hi=min(hi,b.va+len(b.code)); rows=[]
 for ins in md.disasm(b.code[lo-b.va:hi-b.va],lo):
  if not ins.id:continue
  row={"va":hex(ins.address),"op":ins.mnemonic,"args":ins.op_str}
  if ins.mnemonic in ("bl","blx","b") and ins.operands and ins.operands[0].type==ARM_OP_IMM:
   row["target"]=hex(ins.operands[0].imm);row["labels"]=b.labels(ins.operands[0].imm)
  for o in ins.operands:
   if o.type==ARM_OP_MEM and o.mem.base==ARM_REG_PC and not o.mem.index:
    addr=(ins.address+8+o.mem.disp)&0xffffffff
    row["literal_slot"]=hex(addr);w=b.word(addr)
    if w is not None:row["literal_value"]=hex(w);row["literal_labels"]=b.labels(w)
  try:
   r,w=ins.regs_access();row["reads"]=[ins.reg_name(x) for x in r];row["writes"]=[ins.reg_name(x) for x in w]
  except Exception:pass
  rows.append(row)
 return rows

def unwind_entries(b):
 s=b.e.get_section_by_name(".ARM.exidx")
 if s is None:return []
 data=s.data();base=int(s["sh_addr"]); out=[]
 for i in range(0,len(data)-7,8):
  w=struct.unpack_from("<I",data,i)[0]&0x7fffffff
  if w&0x40000000:w-=0x80000000
  addr=(base+i+w)&0xffffffff
  if b.va<=addr<b.va+len(b.code):out.append(addr)
 return sorted(set(out))

def calls_to(b,targets):
 out=[]
 for off in range(0,len(b.code)-3,4):
  va=b.va+off;dest=prel_branch(struct.unpack_from("<I",b.code,off)[0],va)
  if dest in targets:out.append({"va":hex(va),"target":hex(dest),"labels":b.labels(dest)})
 return out

def analyze(b,lane):
 if lane=="setter-arguments":
  rows=windows(b,SETTER-192,SETTER+96)
  assert any(r["va"]==hex(SETTER) and "setExecutable" in str(r.get("labels",[])) for r in rows)
  return {"callsite":hex(SETTER),"instructions":rows}
 if lane=="setter-owner":
  entries=unwind_entries(b);before=[x for x in entries if x<=SETTER];after=[x for x in entries if x>SETTER]
  if not before:return {"unwind_boundary_found":False,"reason":"no preceding unwind entry"}
  entry=before[-1];end=after[0] if after else b.va+len(b.code)
  if end-entry>16384:return {"unwind_boundary_found":False,"reason":"candidate interval too wide","interval":[hex(entry),hex(end)]}
  return {"unwind_boundary_found":True,"candidate_entry":hex(entry),"candidate_end":hex(end),"direct_callers":calls_to(b,{entry})[:100],"entry_window":windows(b,entry,min(entry+128,end))}
 if lane=="filename-data":
  # Exact unaligned filename VA found by R105.157; look at real pointer slots,
  # including unaligned file data, then code PC-relative accesses to those slots.
  slots=mapped_words(b,NAME)
  md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True;hits=[]
  for ins in md.disasm(b.code,b.va):
   if not ins.id:continue
   for op in ins.operands:
    if op.type==ARM_OP_MEM and op.mem.base==ARM_REG_PC and not op.mem.index:
     a=(ins.address+8+op.mem.disp)&0xffffffff
     if a in slots:hits.append({"va":hex(ins.address),"slot":hex(a),"mnemonic":ins.mnemonic})
  return {"filename_va":hex(NAME),"absolute_pointer_slots":[hex(x) for x in slots[:100]],"pc_relative_slot_accesses":hits[:100]}
 if lane=="reader-imports":
  # Query actual imported config / file / parser APIs instead of retrying the
  # absent JSONReader symbol. Match names only; no reader proof by name.
  import re
  targets={v:n for v,n in b.plt.items() if re.search(r"(^|_)(open64|fopen64|open|fopen|read|fread|stat|access|getline|readlink)(@|$)|json|config|property|parse",n,re.I)}
  hits=calls_to(b,set(targets))
  return {"candidate_imports":{hex(v):n for v,n in sorted(targets.items())},"direct_call_count":len(hits),"calls":hits[:200]}
 raise ValueError(lane)

if __name__=="__main__":
 lane,root,out=sys.argv[1:]
 b=original(Path(root).resolve(strict=True))
 result=analyze(b,lane)
 report={"lane":lane,"target_sha256":PIN,"result":result,"scope":"STATIC_OBSERVATION_ONLY","runtime_reader_verified":False,"config_effective_verified":False,"installable":False}
 Path(out).write_text(json.dumps(report,indent=2)+"\n")
 print(json.dumps(report,sort_keys=True))
 print("R105158_STATIC_OBSERVATION_ONLY_NO_INSTALL")
