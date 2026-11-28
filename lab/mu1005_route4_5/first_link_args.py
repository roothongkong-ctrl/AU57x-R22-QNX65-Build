#!/usr/bin/env python3
"""Focused OEM DIO receiver callsite and exact driver transport continuation.
No OEM bytes or live claims; inspect only prior confirmed entry neighborhoods."""
import json,sys
from pathlib import Path
from lifecycle_cfg import md
from probe import Binary,load,prel_branch
from capstone.arm import ARM_OP_IMM
import struct
lane,root,out=sys.argv[1:]
root=Path(root)
def window(b,site,pre,post):
 rows=[]
 for a in range(site-pre,site+post+4,4):
  w=b.word(a)
  if w is None:continue
  i=next(md.disasm(w.to_bytes(4,"little"),a,count=1),None)
  if i is None:continue
  v=prel_branch(w,a)
  rows.append({"va":hex(a),"op":i.mnemonic,"args":i.op_str,
               "target_labels":b.labels(v) if v is not None else []})
 return rows
if lane=="receiver-arguments":
 b=Binary(load(root,"dio_manager"))
 sites={0x15ddbc:"AirPlayReceiverServerCreate",0x15df68:"AirPlayReceiverServerSetDelegate",
        0x15df88:"AirPlayReceiverServerControl",0x15dfbc:"AirPlayReceiverServerControl"}
 for a,n in sites.items():
  assert b.plt.get(prel_branch(b.word(a),a))==n,(hex(a),n)
 result={hex(a):{"import":n,"window":window(b,a,48,24)} for a,n in sites.items()}
elif lane=="driver-callback-arguments":
 b=Binary(load(root,"ipod-drvr-iap2.so"))
 names=("transport_receive","transport_send_pkt","transport_recv_pkt","iap2_start_features")
 result={}
 for name in names:
  matches=[(a,n) for a,n,s in b.funcs if s==name]
  if not matches:raise ValueError("missing "+name)
  a,n=matches[0]
  # Only indirect callback dispatch and its argument setup, not entire prior map.
  for off in range(a,a+n,4):
   w=b.word(off)
   if w is None:continue
   i=next(md.disasm(w.to_bytes(4,"little"),off,count=1),None)
   if i and i.mnemonic.startswith("blx") and i.operands and i.operands[0].type!=ARM_OP_IMM:
    result[hex(off)]={"function":name,"window":window(b,off,32,12)}
 if not result:raise ValueError("no indirect dispatch found")
else:raise ValueError(lane)
report={"lane":lane,"result":result,"scope":"STATIC_ARGUMENT_CANDIDATES_ONLY","runtime_verified":False,"installable":False}
Path(out).write_text(json.dumps(report,indent=2)+"\n")
print("OEM_ARGUMENT_CANDIDATES",json.dumps(report))
