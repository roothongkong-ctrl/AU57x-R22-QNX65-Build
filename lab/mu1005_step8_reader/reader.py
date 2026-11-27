#!/usr/bin/env python3
"""Step 8: bounded data-flow at the four existing original reader callsites."""
import json,sys,re
from pathlib import Path
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_REG,ARM_OP_IMM,ARM_OP_MEM,ARM_REG_PC,ARM_CC_AL,ARM_CC_INVALID
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_launcher_next"))
from next import original,windows,calls_to,PIN
from trace import trace,selftest
LANES={"parser-a":(0x11603c,"parseFileToArray"),"parser-b":(0x1179ac,"parseFileToArray"),"provider":(0x1280f0,"Provider"),"file-open":(0x110454,"fopen64")}
MASK=0xffffffff
def cstring(b,a):
 for off,va,n in b.loads:
  if va<=a<va+n:
   d=b.d[off+a-va:off+min(n,a-va+192)]
   s,sep,_=d.partition(b"\0")
   if sep and 3<=len(s)<=160 and all(32<=x<127 for x in s):
    return s.decode("ascii")
 return None
def recover(b,lo,hi,call):
 md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
 vals={};history=[];args={};last=None
 for ins in md.disasm(b.code[lo-b.va:hi-b.va],lo):
  if last is not None and last!=ins.address:vals.clear()
  last=ins.address+ins.size
  if not ins.id:vals.clear();continue
  op=ins.operands;m=ins.mnemonic
  if ins.address==call:
   args={ins.reg_name(r):{"value":hex(v),"literal_string":cstring(b,v),"origin":hex(at)} for r,(v,at) in vals.items() if ins.reg_name(r) in ("r0","r1","r2","r3")}
  if m in ("bl","blx"):
   # Calls may modify flags and caller-saved registers. Do not infer returned values.
   vals={r:v for r,v in vals.items() if ins.reg_name(r) in ("r4","r5","r6","r7","r8","sb","sl","fp","r9","r10","r11")}
   continue
  if m in ("b","bx","pop") or ins.cc not in (ARM_CC_AL,ARM_CC_INVALID):
   vals.clear();continue
  def val(o):
   if o.shift.type:return None
   if o.type==ARM_OP_IMM:return o.imm&MASK
   if o.type==ARM_OP_REG:return (ins.address+8)&MASK if o.reg==ARM_REG_PC else vals.get(o.reg,(None,0))[0]
  dst=op[0].reg if op and op[0].type==ARM_OP_REG else None
  result=None
  if dst and dst!=ARM_REG_PC:
   if m in ("mov","movw","adr") and len(op)==2:result=val(op[1])
   elif m=="movt" and len(op)==2:
    a=vals.get(dst,(None,0))[0];v=val(op[1])
    if a is not None and v is not None:result=(a&0xffff)|(v<<16)
   elif m in ("add","sub") and len(op)==3:
    a,v=val(op[1]),val(op[2])
    if a is not None and v is not None:result=a+v if m=="add" else a-v
   elif m=="ldr" and len(op)==2 and op[1].type==ARM_OP_MEM and not ins.writeback:
    mem=op[1].mem;a=ins.address+8 if mem.base==ARM_REG_PC else vals.get(mem.base,(None,0))[0]
    if a is not None and not mem.index:result=b.word((a+mem.disp)&MASK)
  try:_,writes=ins.regs_access()
  except Exception:vals.clear();continue
  for r in writes:vals.pop(r,None)
  if ARM_REG_PC in writes:vals.clear();continue
  if result is not None:
   result&=MASK;vals[dst]=(result,ins.address)
   s=cstring(b,result)
   if s and re.search(r"json|config|connectivity|production|/etc/|launcher|bluetooth|enableIap",s,re.I):
    history.append({"va":hex(ins.address),"reg":ins.reg_name(dst),"address":hex(result),"string":s})
 return args,history
def main(lane,root,out):
 b=original(root);call,name=LANES[lane]
 rows=windows(b,call-768,call+160)
 hit=[r for r in rows if r["va"]==hex(call)]
 assert len(hit)==1 and name in str(hit[0].get("labels",[])),hit
 owner=b.owner(call)
 # Use real symbol bounds if available; otherwise a bounded backward window only.
 lo=max(b.va,call-768)
 if owner:lo=max(lo,int(owner["va"],16))
 args,history=recover(b,lo,call+4,call)
 local_calls=[r for r in rows if r["op"] in ("bl","blx")]
 report={"scope":"STATIC_CALLSITE_DATAFLOW_CANDIDATES_ONLY","sha256":PIN,"lane":lane,"call":hit[0],"owner":owner,"linear_argument_candidates":args,"local_config_string_candidates":history,"nearby_calls":local_calls,"instructions":rows,"runtime_read_verified":False,"actual_file_association_verified":False,"installable":False,"limitations":["bounded linear ARM analysis, not a full CFG","C++ object and stack argument origins not resolved by constant propagation","no actual file open or OEM execution"]}
 out.write_text(json.dumps(report,indent=2)+"\n")
 print(json.dumps({k:v for k,v in report.items() if k!="instructions"},sort_keys=True))
 print("R105165_STEP8_CALLSITE_OBSERVED_NOT_RUNTIME")
if __name__=="__main__":
 if sys.argv[1:]==["--selftest"]:selftest();assert len(LANES)==4
 else:main(sys.argv[1],Path(sys.argv[2]).resolve(strict=True),Path(sys.argv[3]))
