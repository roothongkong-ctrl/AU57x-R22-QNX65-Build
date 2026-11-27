#!/usr/bin/env python3
"""Target exact new callee entries, never re-scan old markers or publish OEM bytes."""
import json,sys,re,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary,load
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM,ARM_OP_MEM,ARM_OP_REG,ARM_REG_PC
def targets(root):
 b=Binary(load(root,"bluetooth"));md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
 # R105.145 proved calls to these two entries; previously only caller windows were decoded.
 out=[]
 for a in (0x21d814,0x21cc68):
  assert b.va<=a<b.va+len(b.code)
  sec=b.e.get_section_by_name(".ARM.exidx")
  starts=[]
  if sec is not None:
   import struct
   for off in range(0,len(sec.data())-7,8):
    v=int(sec["sh_addr"])+off;w=struct.unpack_from("<I",sec.data(),off)[0]&0x7fffffff
    if w&0x40000000:w-=0x80000000
    starts.append((v+w)&0xffffffff)
  ends=[v for v in starts if v>a]
  limit=min(a+512,min(ends,default=a+512),b.va+len(b.code))
  ins=[]
  for i in md.disasm(b.code[a-b.va:limit-b.va],a):
   if not i.id:continue
   row={"va":hex(i.address),"mnemonic":i.mnemonic,"operands":i.op_str}
   if i.mnemonic.startswith(("bl","b")) and i.operands and i.operands[0].type==ARM_OP_IMM:
    dest=i.operands[0].imm
    row["target_labels"]=b.labels(dest)
   for op in i.operands:
    if op.type==ARM_OP_MEM and op.mem.base==ARM_REG_PC and not op.mem.index:
     slot=(i.address+8+op.mem.disp)&0xffffffff
     row["literal_slot"]=hex(slot)
     row["slot_relocations"]=b.reloc.get(slot,[])
     value=b.word(slot)
     if value is not None:row["literal_target_labels"]=b.labels(value)
   ins.append(row)
  assert ins
  out.append({"entry":hex(a),"scope":"bounded_callee_not_proved_reader","exidx_entry":a in starts,"end":hex(limit),"instructions":ins})
 # Export-name candidates only, restricted to already confirmed direct dependencies.
 from elftools.elf.elffile import ELFFile
 from elftools.elf.sections import SymbolTableSection
 libs=[]
 for name in ("libosal.so","libutil.so","libcomm.so","libiplcommon.so"):
  seen=set()
  for p in root.rglob(name):
   if not p.is_file() or not p.resolve().is_relative_to(root):continue
   d=hashlib.sha256(p.read_bytes()).hexdigest()
   if d in seen:continue
   seen.add(d)
   with p.open("rb") as f:
    if f.read(4)!=b"\x7fELF":continue
    f.seek(0);e=ELFFile(f)
    sym=e.get_section_by_name(".dynsym")
    names=sorted({s.name for s in sym.iter_symbols() if s["st_shndx"]!="SHN_UNDEF" and re.search(r"config|property|setting",s.name,re.I)}) if isinstance(sym,SymbolTableSection) else []
    libs.append({"library":name,"sha256":d,"candidate_exports":names[:100]})
 return {"callees":out,"candidate_library_exports":libs}
def boot(root):
 # Only boot/config-path statements in text startup/config scripts; no raw lines or credentials.
 records=[];scanned=0
 needle=(b"/mnt/system",b"/etc/eso/production",b"connectivity.json")
 for p in root.rglob("*"):
  if not p.is_file() or p.is_symlink() or not p.resolve().is_relative_to(root):continue
  if p.stat().st_size>1024*1024 or p.name=="connectivity.json":continue
  rel=str(p.relative_to(root))
  if not re.search(r"(^|/)(etc|bin|sbin|scripts|startup|boot)(/|$)",rel,re.I):continue
  d=p.read_bytes()
  if b"\x00" in d or d.startswith(b"\x7fELF"):continue
  scanned+=1
  for i,line in enumerate(d.splitlines(),1):
   hits=[x.decode() for x in needle if x in line]
   if hits and re.search(rb"\b(mount|ln|link|symlink|export|connectivity\.json)\b",line):
    records.append({"file":rel,"sha256":hashlib.sha256(d).hexdigest(),"line":i,"path_tokens":hits,"operation_tokens":[x.decode() for x in (b"mount",b"ln",b"symlink",b"export") if re.search(rb"\b"+x+rb"\b",line)]})
 return {"text_files_scanned":scanned,"candidate_statements":records[:200],"candidate_count":len(records)}
lane,root,out=sys.argv[1:];root=Path(root).resolve(strict=True)
r=targets(root) if lane=="reader-targets" else boot(root) if lane=="boot-path" else None
assert r is not None
report={"lane":lane,"result":r,"actual_config_reader_verified":False,"vehicle_verified":False}
Path(out).write_text(json.dumps(report,indent=2)+"\n")
if lane=="reader-targets":
 for row in r["callees"]:
  print("CALLEE",row["entry"],"EXIDX",row["exidx_entry"])
  for i in row["instructions"]:print(i["va"],i["mnemonic"],i["operands"],i.get("target_labels",[]),i.get("literal_target_labels",[]))
 for row in r["candidate_library_exports"]:print("EXPORT_CANDIDATES",row["library"],row["candidate_exports"])
else:print(json.dumps(r))
print("R105156_EVIDENCE_ONLY_NOT_CONSUMER_VERIFIED")
