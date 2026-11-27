#!/usr/bin/env python3
"""Find explicit original launcher argv in extracted startup descriptors, never execute scripts."""
import json,re,shlex,sys,hashlib
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2]);header=Path(sys.argv[3])
hits=[];commands={}
name="connectivity_launcher"
flags={"-sm-name","-sm-id","-sm-time","-sm-restart"}
for p in sorted(root.rglob("*")):
 if not p.is_file() or p.is_symlink():continue
 if not p.resolve().is_relative_to(root) or p.stat().st_size>2*1024*1024:continue
 if p.suffix.lower() not in (".sh",".cfg",".conf",".json",".xml",".ini",".txt",".rc",".build",".properties",".lst",".script","") and not p.name.startswith(("startup","boot","launch")):continue
 d=p.read_bytes()
 if b"\0" in d or name.encode() not in d:continue
 lines=d.decode("utf-8",errors="replace").splitlines()
 for i,line in enumerate(lines):
  if name not in line:continue
  context="\n".join(lines[max(0,i-2):i+3])
  hits.append({"path":str(p.relative_to(root)),"sha256":hashlib.sha256(d).hexdigest(),"line":i+1,"context":context[:800]})
  if any(x in line for x in ("$",";","|","&","<",">",chr(96),"\\","{","}","(",")")):continue
  try:a=shlex.split(line,comments=True)
  except ValueError:continue
  if not a or a[0].rsplit("/",1)[-1]!=name:continue
  if len(a)!=9 or set(a[1::2])!=flags:continue
  pairs=dict(zip(a[1::2],a[2::2]))
  if pairs["-sm-name"]!=name or not all(re.fullmatch(r"[0-9]+",pairs[x]) for x in flags-{"-sm-name"}):continue
  commands.setdefault(tuple([name]+a[1:]),[]).append({"path":str(p.relative_to(root)),"line":i+1})
lib=[]
for p in sorted(root.rglob("libservmngt.so")):
 if not p.is_file() or not p.resolve().is_relative_to(root):continue
 d=p.read_bytes();sha=hashlib.sha256(d).hexdigest()
 if any(x["sha256"]==sha for x in lib):continue
 markers=[m.decode("ascii",errors="replace") for m in re.findall(rb"[\x20-\x7e]{4,180}\x00",d) if b"bundle loader commandline" in m or b"AgentStarter" in m or b"-sm-" in m]
 lib.append({"path":str(p.relative_to(root)),"sha256":sha,"markers":[m.rstrip("\0") for m in markers[:40]]})
r={"scope":"ORIGINAL_BOOT_ARGUMENT_EVIDENCE_ONLY","descriptor_hits":hits[:100],"hit_count":len(hits),"explicit_commands":[{"argv":list(a),"sources":s} for a,s in commands.items()],"service_library":lib,"runtime_verified":False}
out.write_text(json.dumps(r,indent=2)+"\n")
print(json.dumps({"scope":r["scope"],"hit_count":len(hits),"descriptor_hits":hits[:16],"explicit_commands":r["explicit_commands"],"service_library":lib},sort_keys=True))
if len(commands)!=1:
 print("STEP8_STOCK_ARGV_NOT_UNIQUE_NO_GUESSED_LAUNCH")
 sys.exit(3)
a=next(iter(commands))
header.write_text("static char *stock_args[]={"+",".join(json.dumps(v) for v in a)+",NULL};\n")
print("STEP8_EXPLICIT_FACTORY_ARGV_FOUND")
