#!/usr/bin/env python3
"""R88 read-only, SHA-pinned original MU1005 Bluetooth/iAP2 route audit.
No proprietary ELF is copied or modified. No command is executed on the car.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

BT_SHA = "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
APP_SHA = "7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272"
R45_RX_ENTRIES = 40
MARKERS = {
    "joey_MU0678_IapDevice": b"IapDevice",
    "joey_MU0678_IapServices": b"IapServices",
    "joey_MU0678_dev_iapDevice": b"/dev/iapDevice",
    "joey_MU0678_CIapBTChannel": b"CIapBTChannel",
    "original_BT_QuerySdp_vtable": b"_ZTVN7btstack4jobs13BtJobQuerySdpE",
    "original_BT_StartService_vtable": b"_ZTVN7btstack4jobs17BtJobStartServiceE",
    "original_BT_RF_tracing": b"CON_BTSTACK_IA_RFCOMM",
    "original_BT_OBEX_client_event": b"OBEX: Client RFCOMM Event",
    "original_app_enableIap": b"bluetooth.enableIap",
    "original_app_iap2_service": b"SERVICETYPE_IAP2",
    "original_app_iapEnabled": b"iapEnabled",
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_verified(path, sha):
    p=Path(path)
    if p.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("unexpectedly large original executable")
    b=p.read_bytes()
    if sha256(b) != sha:
        raise ValueError("original image hash mismatch: " + p.name)
    return b


def elf32_arm_qnx(b):
    if len(b)<52 or b[:7]!=b"\x7fELF\x01\x01\x01":
        raise ValueError("not ELF32 little-endian")
    if struct.unpack_from("<HH",b,16)!=(2,40):
        raise ValueError("not ARM32 executable")
    phoff=struct.unpack_from("<I",b,28)[0]
    phentsize,phnum=struct.unpack_from("<HH",b,42)
    if phentsize<32 or phnum<2 or phnum>64 or phoff+phentsize*phnum>len(b):
        raise ValueError("invalid program headers")
    loads=[];interp=None
    for i in range(phnum):
        off=phoff+i*phentsize
        kind,fo,va,_,fs,ms,flags,_=struct.unpack_from("<8I",b,off)
        if fo>len(b) or fs>len(b)-fo or fs>ms:
            raise ValueError("invalid segment length")
        if kind==1:
            loads.append((va,va+fs,fo,flags))
        if kind==3:
            interp=b[fo:fo+fs].split(b"\x00",1)[0]
    if interp!=b"/usr/lib/ldqnx.so.2":
        raise ValueError("not original QNX interpreter")
    if not any(flags&1 for _,_,_,flags in loads):
        raise ValueError("no executable mapped segment")
    return loads


def words_from_header(path):
    content=Path(path).read_text(encoding="utf-8")
    entries=[(int(x,16),int(y,16)) for x,y in re.findall(
        r"\{\s*(0x[0-9a-fA-F]+)u\s*,\s*(0x[0-9a-fA-F]+)u\s*\}",content)]
    if len(entries)!=R45_RX_ENTRIES or len({x for x,_ in entries})!=len(entries):
        raise ValueError("R45 words expected exactly 40 unique ARM instructions")
    return entries


def check_words(data,loads,entries):
    checked=[]
    for va,want in entries:
        loc=None
        for begin,end,off,flags in loads:
            if flags&1 and begin<=va and va+4<=end:
                loc=off+va-begin;break
        if loc is None:
            raise ValueError("R45 word not in original executable segment")
        got=struct.unpack_from("<I",data,loc)[0]
        if got!=want:
            raise ValueError("R45 pinned instruction differs: VA 0x%x" % va)
        checked.append("0x%08x" % va)
    return checked


def scan(data,selected):
    return {key:{"present":data.find(MARKERS[key])>=0,
                 "file_offset_hex":("0x%x" % data.find(MARKERS[key]) if data.find(MARKERS[key])>=0 else None)}
            for key in selected}


def audit(btpath,apppath,wordpath):
    bt=read_verified(btpath,BT_SHA)
    app=read_verified(apppath,APP_SHA)
    loads=elf32_arm_qnx(bt)
    elf32_arm_qnx(app)
    checked=check_words(bt,loads,words_from_header(wordpath))
    bt_marks=[k for k in MARKERS if k.startswith("joey_") or k.startswith("original_BT_")]
    app_marks=[k for k in MARKERS if k.startswith("original_app_")]
    b=scan(bt,bt_marks);a=scan(app,app_marks)
    if not all(a[k]["present"] for k in app_marks):
        raise ValueError("original app iAP control evidence missing")
    if not all(b[k]["present"] for k in bt_marks if k.startswith("original_BT_")):
        raise ValueError("original btstack RF/SDP/client evidence missing")
    missing=[k for k in bt_marks if k.startswith("joey_") and not b[k]["present"]]
    return {
        "release":"R88","baseline":"MHI2_CN_AU57x_K1006_2/MU1005",
        "audit_type":"pinned_original_static_readonly","originals_kept_private":True,
        "btstack_sha256":BT_SHA,"bluetooth_sha256":APP_SHA,
        "r45_checked_arm_instructions":len(checked),
        "r45_checked_vas":checked,
        "btstack_markers":b,"bluetooth_markers":a,
        "joey_MU0678_path_not_found_markers":missing,
        "mu1005_existing_iapDevice_path_proven":False,
        "mu1005_stock_rfcomm_server_code_proven":True,
        "mu1005_obex_client_code_indicated":True,
        "mu1005_client_open_abi_proven":False,
        "recommended_next_path":"R45 native server ingress -> owner-bounded R87 -> R44 stock MFi",
        "qualification":"Missing a literal is not proof the route cannot be generated dynamically. Original BT RFCOMM/client snippets do not prove RF_OpenClientChannel ABI or a live phone. The 40 R45 words prove image consistency, not safe runtime binding.",
        "installable":False,"safe_to_flash":False,"vehicle_verified":False
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--btstack",required=True)
    p.add_argument("--bluetooth",required=True)
    p.add_argument("--r45-words",required=True)
    p.add_argument("--out",required=True)
    args=p.parse_args()
    obj=audit(args.btstack,args.bluetooth,args.r45_words)
    Path(args.out).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("R88 pinned MU1005 static path audit: PASS; original ELFs excluded from result")

if __name__=="__main__":main()
