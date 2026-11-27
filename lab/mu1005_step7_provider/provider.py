#!/usr/bin/env python3
"""Read the actual original ProcessTNG provider from the pinned archive."""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_launcher_next"))
from next import original,windows,calls_to,Binary
root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
launcher=original(root)
needed=launcher.e.get_section_by_name(".dynamic")
names=[t.needed for t in needed.iter_tags() if t.entry.d_tag=="DT_NEEDED"]
candidates={}
for name in names:
 if "osal" not in name.lower():continue
 for p in root.rglob(name):
  if not p.is_file() or not p.resolve().is_relative_to(root):continue
  data=p.read_bytes()
  if data[:6]!=b"\x7fELF\x01\x01":continue
  b=Binary(data)
  funcs=[(v,n,s) for v,n,s in b.funcs if s=="_ZN4osal10ProcessTNG5startEv"]
  if funcs:candidates[hashlib.sha256(data).hexdigest()]=(name,b,funcs)
if len(candidates)!=1:raise ValueError("expected unique original ProcessTNG provider, found "+str(len(candidates)))
sha,(name,b,funcs)=next(iter(candidates.items()))
entries=sorted(set((v,n) for v,n,s in funcs))
assert len(entries)==1
v,n=entries[0]
assert 0<n<=65536
rows=windows(b,v,v+n)
calls=[r for r in rows if r["op"] in ("bl","blx")]
report={"scope":"ORIGINAL_OSAL_START_STATIC_ONLY","provider":name,"sha256":sha,"start_va":hex(v),"size":n,"calls":calls[:150],"argument_or_spawn_imports":{hex(a):s for a,s in b.plt.items() if any(x in s.lower() for x in ("spawn","execv","argument","argv","param"))},"runtime_verified":False,"installable":False}
out.write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,sort_keys=True))
print("R105161_OSAL_PROVIDER_STATIC_ONLY")
