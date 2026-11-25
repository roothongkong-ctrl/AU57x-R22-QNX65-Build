#!/usr/bin/env python3
"""Private MU1005 btstack: locate candidate trace/startup/ACL references.
Reports addresses only; no OEM bytes, patch, runtime calls or install.
ARM direct literals/ADR only, not Thumb/PIC/GOT/indirect data flow.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from mu1005_bt_iap_path_audit import BT_SHA, elf32_arm_qnx

MARKERS = {
    "device_id": (b"Device ID", b"Device-ID", b"DeviceID", b"Device Id"),
    "acl_up": (b"ACL to ", b"ACL connection", b"ACL established", b"already connected"),
    "rfcomm_trace": (b"CON_BTSTACK_IA_RFCOMM",),
    "sdp_query": (b"BtJobQuerySdp",),
    "start_service": (b"BtJobStartService",),
}
RF_REGISTER = 0x277a18

def rot32(x, n):
    return ((x >> n) | (x << (32-n))) & 0xffffffff if n else x

def branch_target(va, op):
    # ARM B/BL (not unconditional BLX-immediate, condition 0xf).
    if op >> 28 == 15 or (op & 0x0e000000) != 0x0a000000:
        return None
    d = (op & 0xffffff) << 2
    if d & (1 << 25):
        d -= 1 << 26
    return (va + 8 + d) & 0xffffffff

def direct_address(va, op, read32):
    if op >> 28 == 15:
        return None
    # Immediate offset LDR word from PC, no writeback, pre-index.
    if (op & 0x0f7f0000) == 0x051f0000:
        off = op & 0xfff
        literal = va + 8 + (off if op & (1 << 23) else -off)
        return read32(literal)
    # ADR = ADD/SUB immediate with PC base, no flag update.
    if (op & 0x0fff0000) in (0x028f0000, 0x024f0000):
        n = rot32(op & 255, ((op >> 8) & 15)*2)
        return (va + 8 + (n if op & (1 << 22) == 0 else -n)) & 0xffffffff
    return None

def scan(blob, loads):
    def read32(va):
        for a,b,f,_ in loads:
            if a <= va and va+4 <= b:
                return struct.unpack_from("<I",blob,f+va-a)[0]
        return None
    hits = {}
    for tag, needles in MARKERS.items():
        for needle in needles:
            for a,b,f,_ in loads:
                lo=f; hi=f+b-a
                while True:
                    off=blob.find(needle,lo,hi)
                    if off<0: break
                    # Trace strings may be a substring of a format string:
                    # recover its bounded printable start, not a raw dump.
                    start=off
                    while start>f and off-start<256 and 32<=blob[start-1]<=126:
                        start-=1
                    for pos in (off,start):
                        hits.setdefault(a+pos-f,set()).add(tag)
                    lo=off+1
    refs=[]; branches=[]
    for a,b,f,flags in loads:
        if not flags&1: continue
        for va in range((a+3)&~3,b-3,4):
            op=read32(va)
            if op is None: continue
            addr=direct_address(va,op,read32)
            if addr in hits:
                refs.append({"site":hex(va),"string_va":hex(addr),"tags":sorted(hits[addr])})
            dest=branch_target(va,op)
            if dest==RF_REGISTER:
                branches.append({"site":hex(va),"kind":"BL" if op&(1<<24) else "B"})
    return {"markers":[{"va":hex(k),"tags":sorted(v)} for k,v in sorted(hits.items())],
            "direct_literal_or_adr_candidates":refs,"direct_rf_register_branches":branches}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--btstack",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    a=p.parse_args()
    if a.out.exists(): p.error("output already exists")
    if a.btstack.stat().st_size>16*1024*1024: p.error("unexpected size")
    blob=a.btstack.read_bytes()
    digest=hashlib.sha256(blob).hexdigest()
    if digest!=BT_SHA: p.error("not exact MU1005 btstack")
    report=scan(blob,elf32_arm_qnx(blob))
    report.update({"sha256":digest,"scope":"STATIC_CANDIDATES_ONLY",
        "limitations":"Data in executable segments can resemble instructions; missing direct refs do not rule out PIC/GOT, Thumb or indirect callers.",
        "owner_entry_verified":False,"installable":False,"vehicle_verified":False})
    with a.out.open("x",encoding="utf-8") as f:
        json.dump(report,f,indent=2);f.write("\n")
    print("Trace markers:",len(report["markers"]),"direct candidates:",len(report["direct_literal_or_adr_candidates"]))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
