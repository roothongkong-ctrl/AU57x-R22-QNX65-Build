#!/usr/bin/env python3
"""Read the already-uploaded exact OEM ELF and verify original startup callsite."""
import hashlib, json, pathlib, re, struct, subprocess, sys
from mu1005_bt_iap_path_audit import BT_SHA, elf32_arm_qnx
from mu1005_bt_trace_entry import branch_target
p=pathlib.Path(sys.argv[1]); data=p.read_bytes()
if hashlib.sha256(data).hexdigest()!=BT_SHA: sys.exit("WRONG_OEM_ELF")
loads=elf32_arm_qnx(data)
def at(va,n):
    for a,b,f,_ in loads:
        if a<=va and va+n<=b: return data[f+va-a:f+va-a+n]
    raise ValueError(hex(va))
def word(va): return struct.unpack("<I",at(va,4))[0]
assert word(0x11ed10)==0xebffe04b
assert branch_target(0x11ed10,word(0x11ed10))==0x116e44
assert b"Primary DID registered" in at(0x27cf94,64)
assert word(0x11ec94)==0xe3a03001
# Verify all ten runtime provenance constants against the exact stock image,
# not against a synthetic fixture. The started flag is STRB, not STR.
sites={0x2be500:0x11e9b0,0x11e9b0:0xe92d4ff0,0x11eab0:0xeb0571b6,
       0x11ec94:0xe3a03001,0x11ec98:0xe5c531b0,0x125908:0xe92d4070,
       0x125968:0xeb055604,0x125970:0xe5c531b0,
       0x11de34:0xeb0574d1,0x11de54:0xe5c831b0}
for va,expected in sites.items():
    assert word(va)==expected, (hex(va),hex(word(va)),hex(expected))
assert branch_target(0x125968,word(0x125968))==0x27b180
assert branch_target(0x11de34,word(0x11de34))==0x27b180
dyn=subprocess.check_output(["readelf","-Wd",str(p)],text=True)
needed=re.findall(r"\(NEEDED\).*?\[([^\]]+)\]",dyn)
assert len(needed)==11 and len(set(needed))==11
print("OEM_DIRECT_DEPENDENCIES",json.dumps(needed))
print("OEM_LIFECYCLE_10_SITES_VERIFIED_STATIC_ONLY")
rel=subprocess.check_output(["readelf","-Wr",str(p)],text=True)
assert re.search(r"(?im)^0*2c12c8\s+.*R_ARM_JUMP_SLOT.*_ZN7tracing7Channel5logCVEsPKcz",rel), "original trace relocation not verified"
print(json.dumps({"sha256":BT_SHA,"trace_call":"0x11ed10","trace_plt":"0x116e44","trace_jump_slot":"0x2c12c8","started_store":"0x11ec98","static_only":True,"oem_runtime_verified":False},indent=2))
print("MU1005_REAL_OEM_TRACE_ANCHORS_PASS_STATIC_ONLY")
