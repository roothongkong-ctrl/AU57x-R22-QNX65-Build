#!/usr/bin/env python3
"""Independent original native provider candidates; no OEM bytes exported.
Each lane examines a distinct unresolved interface, not historical fixtures."""
import hashlib,json,re,sys
from pathlib import Path
from lifecycle_cfg import walk
from probe import Binary,load
root=Path(sys.argv[2]).resolve(strict=True);lane=sys.argv[1];out=Path(sys.argv[3])
def exact(name,pin=None):
 for p in sorted(root.rglob(name)):
  if p.is_file() and p.resolve().is_relative_to(root):
   d=p.read_bytes()
   if d[:6]==b"\x7fELF\x01\x01" and (not pin or hashlib.sha256(d).hexdigest()==pin):return Binary(d),hashlib.sha256(d).hexdigest()
 raise ValueError("missing original "+name)
def imports(b,pattern):
 rx=re.compile(pattern,re.I)
 return [{"slot":hex(a),"symbol":r["symbol"],"type":r["type"],"plt":[hex(v) for v,n in b.plt.items() if n==r["symbol"]]} for a,rs in b.reloc.items() for r in rs if r["symbol"] and rx.search(r["symbol"])]
def calls(b,rows):
 import struct
 from probe import prel_branch
 targets={int(v,16) for r in rows for v in r["plt"]}
 return [{"site":hex(b.va+o),"target":hex(v),"labels":b.labels(v),"owner":b.owner(b.va+o)} for o in range(0,len(b.code)-3,4) if (v:=prel_branch(struct.unpack_from("<I",b.code,o)[0],b.va+o)) in targets]
if lane=="stop-sync":
 b=Binary(load(root,"btstack"))
 # Actual first cleanup tailcall and second cleanup lock acquisition/release,
 # based on R105.207, not repeated parent routine scan.
 ranges={"first_tail":(0x278418,0x278418,0x278448),"second_lock":(0x27a12c,0x27a12c,0x27a1b8),"second_unlock":(0x27a0a0,0x27a0a0,0x27a12c)}
 result={k:walk(b.word,*v,b.labels) for k,v in ranges.items()}
elif lane=="driver-producer":
 b=Binary(load(root,"ipod-drvr-iap2.so"))
 # Resolve *imports* and direct call owners for dynamic transport discovery
 # and resmgr endpoints; descriptor offsets were already verified.
 rows=imports(b,r"dlopen|dlsym|resmgr_|name_attach|open|connect|TransportSupportsiAP2Connection|BluetoothTransport")
 result={"imports":rows,"call_owners":calls(b,rows),"note":"direct PLT callers only; dynamic producer identity not inferred"}
elif lane=="receiver-provider":
 b=Binary(load(root,"dio_manager"))
 rows=imports(b,r"AirPlayReceiverServer(Get|Set|Control|Create)|DNSService|if_nametoindex|CFString|APSMFiPlatform")
 result={"imports":rows,"call_owners":calls(b,rows),"note":"actual DIO caller candidates; no library handle or OEM owner assumed"}
elif lane=="si-provider":
 b=Binary(load(root,"smartphone_integrator"))
 rows=imports(b,r"AirPlay|DNSService|if_nametoindex|MFi|CarPlay|iap2|requestPrepareConnect")
 result={"imports":rows,"call_owners":calls(b,rows),"note":"actual SI imported interfaces and call owners; unresolved dynamic dispatch not excluded"}
else:raise ValueError("unknown lane")
report={"lane":lane,"sha256":hashlib.sha256(b.d).hexdigest(),"result":result,"runtime_verified":False,"installable":False}
out.write_text(json.dumps(report,indent=2)+"\n")
if lane=="stop-sync":
 for k,v in result.items():print("NATIVE_LANE",k,json.dumps(v))
else:
 print("NATIVE_IMPORTS",lane,json.dumps(result["imports"]))
 print("NATIVE_CALL_OWNERS",lane,json.dumps(result["call_owners"][:150]))
print("R105208_DISTINCT_NATIVE_LANE_COMPLETE",lane)
