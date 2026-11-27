#!/usr/bin/env python3
"""Derive physical transport argument evidence, not an executable OEM ABI."""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2])
def original(name,pin):
    for f in sorted(root.rglob(name)):
        if f.is_file() and f.resolve().is_relative_to(root):
            data=f.read_bytes()
            if hashlib.sha256(data).hexdigest()==pin:
                return Binary(data)
    raise ValueError("missing pinned original "+name)
dr=original("ipod-drvr-iap2.so","42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf")
usb=original("ipod-transport-usbdevice.so","e1743821b166fcd93992b45fc7064ad14763ee8a4445478e9aa36e696f2f6e95")
md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
def window(b,addr,n):
    if n<=0 or n>4096 or n%4 or addr%4 or not b.va<=addr<addr+n<=b.va+len(b.code):
        raise ValueError("unbounded code window")
    marks=sorted((a,s.split(".")[0]) for a,ns in b.names.items() for s in ns if s.split(".")[0] in ("$a","$d","$t"))
    rows=[]
    for a in range(addr,addr+n,4):
        mode=next((m for v,m in reversed(marks) if v<=a),None)
        i=None if mode in ("$d","$t") else next(md.disasm(b.code[a-b.va:a-b.va+4],a,count=1),None)
        row={"va":hex(a),"mapping":mode}
        if i is None:
            row["gap"]=True
        else:
            row["insn"]=i.mnemonic+" "+i.op_str
            if i.mnemonic in ("bl","blx","b") and i.operands and i.operands[0].type==ARM_OP_IMM:
                target=i.operands[0].imm
                row.update(target=hex(target),labels=b.labels(target))
        rows.append(row)
    return rows
def sized(b,addr,name):
    ns={n for a,n,s in b.funcs if a==addr and s==name and n>0}
    if len(ns)!=1:raise ValueError("missing exact function "+name)
    return ns.pop()
# R105.012 already proved the slot offsets. This pass preserves argument
# setup and return handling at the callers and USB donor entry instructions.
spec=[("driver_receive_args",dr,0x2111c,40),("driver_send_args",dr,0x2115c,192),
      ("raw_send_backend",dr,0x113a8,sized(dr,0x113a8,"link_send_raw_message")),
      ("get_transport_data",dr,0x12208,sized(dr,0x12208,"link_get_transport_data")),
      ("usb_send_entry",usb,0x1560,96),("usb_recv_entry",usb,0x1228,96)]
res={"scope":"STATIC_ARGUMENT_AND_RETURN_CONTRACT_CANDIDATES","prior_run":46025407820,"runtime_verified":False,"wireless_producer_verified":False,"windows":{}}
for key,b,addr,n in spec:
    rows=window(b,addr,n)
    res["windows"][key]={"entry":hex(addr),"symbols":b.labels(addr),"rows":rows}
out.write_text(json.dumps(res,indent=2)+"\n")
for key in ("driver_receive_args","driver_send_args","usb_send_entry","usb_recv_entry"):
    print("ROUTE3_ARGUMENT_WINDOW",key,json.dumps(res["windows"][key]))
for key in ("raw_send_backend","get_transport_data"):
    print("ROUTE3_BACKEND_CALLS",key,json.dumps([r for r in res["windows"][key]["rows"] if r.get("target")]))
assert all("insn" in w["rows"][0] for w in res["windows"].values())
print("ROUTE3_CONTRACT_WINDOWS_RETAINED_NOT_LIVE_ABI")
