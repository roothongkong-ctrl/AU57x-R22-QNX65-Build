#!/usr/bin/env python3
"""Generate exact caller guards from pinned original DIO, not from donor addresses."""
import struct,sys
from pathlib import Path
from probe import Binary,load,prel_branch
b=Binary(load(Path(sys.argv[1]).resolve(strict=True),"dio_manager"))
targets={v for v,n in b.plt.items() if n=="AirPlayReceiverServerControl"}
print("DIO_AIRPLAY_IMPORTS",sorted((hex(v),n) for v,n in b.plt.items() if "AirPlayReceiverServer" in n))
if len(targets)!=1:raise SystemExit("DIO original control import missing or ambiguous")
rows=[]
for o in range(0,len(b.code)-3,4):
    a=b.va+o;w=struct.unpack_from("<I",b.code,o)[0]
    if prel_branch(w,a) in targets:rows.append((a,w))
if not rows or len(rows)>64:raise SystemExit("No bounded original callsites")
Path(sys.argv[2]).write_text("/* generated from exact MU1005 DIO ELF */\n"+
    "\n".join("{0x%08xu,0x%08xu},"%(a,w) for a,w in rows)+"\n")
print("ORIGINAL_DIO_CONTROL_CALLS",[(hex(a),hex(w)) for a,w in rows])
