#!/usr/bin/env python3
"""Target only the unresolved stock DNS-SD service provider and its boot reference."""
import hashlib,json,re,sys,subprocess
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
names={"mdnsd","mDNSResponder","mDNSResponderPosix","libdns_sd.so.1"}
files=[];starts=[]
for p in sorted(root.rglob("*")):
 if not p.is_file() or not p.resolve().is_relative_to(root):continue
 if p.name in names:
  d=p.read_bytes()
  if d[:6]!=b"\x7fELF\x01\x01" or d[18:20]!=b"\x28\x00":continue
  dyn=subprocess.run(["readelf","-dW",str(p)],capture_output=True,text=True,timeout=30)
  paths=sorted(set(x.decode("ascii") for x in re.findall(rb"/(?:var/run|dev/socket|tmp|dev/shmem)/[A-Za-z0-9_.-]*(?:[Mm][Dd][Nn][Ss]|[Bb]onjour|[Dd][Nn][Ss])[A-Za-z0-9_.-]*",d)))
  files.append({"path":str(p.relative_to(root)),"sha256":hashlib.sha256(d).hexdigest(),"needed":re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",dyn.stdout),"socket_path_candidates":paths})
 elif p.suffix in (".sh",".json",".cfg",".conf",".rc",".ini",".xml") and p.stat().st_size<2*1024*1024:
  d=p.read_bytes()
  if b"\0" in d:continue
  for i,l in enumerate(d.decode("utf-8",errors="replace").splitlines(),1):
   if re.search(r"\b(?:mdnsd|mDNSResponder(?:Posix)?)\b",l):
    # report source location and matched name, not entire configuration line
    starts.append({"path":str(p.relative_to(root)),"line":i,"names":sorted(set(re.findall(r"\b(?:mdnsd|mDNSResponder(?:Posix)?)\b",l)))})
r={"scope":"STOCK_DNS_SD_PROVIDER_LOCATION_ONLY","binaries":files,"startup_references":starts[:100],"startup_reference_count":len(starts),"runtime_connection_verified":False}
out.write_text(json.dumps(r,indent=2)+"\n")
print(json.dumps(r,sort_keys=True))
if not any(Path(x["path"]).name!="libdns_sd.so.1" for x in files):raise SystemExit("ORIGINAL_DAEMON_NOT_LOCATED")
print("STOCK_DAEMON_BINARY_LOCATED_NOT_STARTED")
