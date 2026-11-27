#!/usr/bin/env python3
"""Trace the new object producer edges; inspect native parser ABI without guessed calls."""
import hashlib,io,json,re,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_launcher_next"))
from next import original,windows,calls_to,PIN
from probe import Binary
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
def edges(root):
 b=original(root)
 # Exact function entry observed at 128058 push; follow its callers, not the old parser calls.
 callers=calls_to(b,{0x128058})
 assert windows(b,0x128058,0x12805c)[0]["op"]=="push"
 # Track two original object slots at parser-b: UnicodeString input at sp+e0
 # and its companion at sp+dc. Keep exact writers and surrounding instructions.
 rows=windows(b,0x117958-0x800,0x117958)
 slots=[r for r in rows if re.search(r"\[sp, #0x(?:dc|e0)\]",r["args"]) and r["op"].startswith(("str","ldr"))]
 # Trace instructions at the immediate producer rather than assume an object field is a filename.
 return {"scope":"NEW_OBJECT_PRODUCER_EDGES_STATIC_ONLY","sha256":PIN,"provider_entry":"0x128058","provider_direct_callers":callers,"caller_windows":[{"caller":r,"window":windows(b,int(r["va"],16)-112,int(r["va"],16)+16)} for r in callers[:20]],"unicode_slot_accesses":slots,"unicode_slot_windows":[windows(b,int(r["va"],16)-28,int(r["va"],16)+12) for r in slots[-16:]],"runtime_file_association":False}
def abi(root):
 seen={};out=[]
 for p in sorted(root.rglob("libutil.so")):
  if not p.is_file() or not p.resolve().is_relative_to(root):continue
  d=p.read_bytes();sha=hashlib.sha256(d).hexdigest()
  if sha in seen:continue
  seen[sha]=str(p.relative_to(root))
  try:b=Binary(d)
  except Exception:continue
  matches=[]
  for v,n,name in b.funcs:
   if not re.search(r"JSONParser(C[12]Ev|D[012]Ev|16parseFileToArray)|ProviderC[12]EPKc",name):continue
   matches.append({"symbol":name,"va":hex(v),"size":n,"entry":windows(b,v,v+min(n,120))})
  if matches:out.append({"path":seen[sha],"sha256":sha,"exports":matches})
 return {"scope":"NATIVE_PARSER_DEFINED_ABI_STATIC_ONLY","libraries":out,"parser_abi_verified":False,"native_parse_executed":False}
if __name__=="__main__":
 lane,root,out=sys.argv[1:];root=Path(root).resolve(strict=True)
 r=edges(root) if lane=="producer" else abi(root)
 r["installable"]=False
 Path(out).write_text(json.dumps(r,indent=2)+"\n")
 # Keep output compact; full disassembly retained in artifact.
 if lane=="producer":print(json.dumps({k:v for k,v in r.items() if not k.endswith("windows")},sort_keys=True))
 else:
  print(json.dumps({"scope":r["scope"],"libraries":[{"path":x["path"],"sha256":x["sha256"],"exports":[{k:v for k,v in s.items() if k!="entry"} for s in x["exports"]]} for x in r["libraries"]]},sort_keys=True))
 print("R105167_NEW_EDGE_OBSERVATION_ONLY")
