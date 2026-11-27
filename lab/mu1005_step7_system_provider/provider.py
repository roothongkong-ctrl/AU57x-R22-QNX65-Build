#!/usr/bin/env python3
"""Find exact defined ProcessTNG::start symbol across app70 and stage2, including SONAME aliases."""
import hashlib,io,json,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_launcher_next"))
from next import original,windows,Binary
root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
symbol="_ZN4osal10ProcessTNG5startEv"
launcher=original(root)
names=[t.needed for t in launcher.e.get_section_by_name(".dynamic").iter_tags() if t.entry.d_tag=="DT_NEEDED"]
found={}; examined=set(); candidates=[]; errors=[]
for p in sorted(root.rglob("*")):
 if not p.is_file() or ".so" not in p.name:continue
 p=p.resolve()
 if not p.is_relative_to(root) or p in examined:continue
 examined.add(p)
 try:
  with p.open("rb") as f:
   if f.read(6)!=b"\x7fELF\x01\x01":continue
   f.seek(0);e=ELFFile(f)
   if e["e_machine"]!="EM_ARM":continue
   dyn=e.get_section_by_name(".dynamic")
   sonames=[t.soname for t in dyn.iter_tags() if t.entry.d_tag=="DT_SONAME"] if dyn else []
   if "osal" in p.name.lower() or any("osal" in s.lower() for s in sonames):
    candidates.append({"path":str(p.relative_to(root)),"sonames":sonames})
   matches=[]
   for sec in e.iter_sections():
    if isinstance(sec,SymbolTableSection):
     matches.extend((int(s["st_value"]),int(s["st_size"])) for s in sec.iter_symbols() if s.name==symbol and s["st_shndx"]!="SHN_UNDEF" and s["st_value"])
   if not matches:continue
  data=p.read_bytes();sha=hashlib.sha256(data).hexdigest()
  if sha in found:
   found[sha]["paths"].append(str(p.relative_to(root)));continue
  b=Binary(data);rows=[]
  for v,n in sorted(set(matches)):
   # For zero-sized definitions, report bounded entry only, not a proved extent.
   size=n if 0<n<=65536 else 256
   ins=windows(b,v&~1,(v&~1)+size) if not v&1 else []
   rows.append({"va":hex(v),"symbol_size":n,"thumb":bool(v&1),"calls":[r for r in ins if r["op"] in ("bl","blx")][:100],"entry":ins[:24]})
  found[sha]={"paths":[str(p.relative_to(root))],"sonames":sonames,"direct_needed":bool(set(names)&set(sonames+[p.name])),"symbols":rows,"spawn_imports":{hex(a):s for a,s in b.plt.items() if any(x in s.lower() for x in ("spawn","execv","argv","argument"))}}
 except Exception as ex:
  if "osal" in p.name.lower():errors.append({"path":str(p.relative_to(root)),"error":str(ex)[:180]})
report={"scope":"STATIC_PROVIDER_DISCOVERY_ONLY","launcher_needed":names,"libraries_examined":len(examined),"osal_candidates":candidates,"providers":found,"errors":errors,"runtime_verified":False,"installable":False}
out.write_text(json.dumps(report,indent=2)+"\n");print(json.dumps(report,sort_keys=True))
if not found:raise SystemExit("NO_DEFINED_PROCESS_TNG_START")
print("R105162_PROVIDER_CANDIDATES_FOUND_NOT_RUNTIME")
