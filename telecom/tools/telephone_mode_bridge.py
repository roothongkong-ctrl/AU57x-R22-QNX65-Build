#!/usr/bin/env python3
"""Exact ARM evidence of the OEM telephone selectNetwork proxy forwarding.

Only verifies immutable instructions in the owner's SHA-checked telephone ELF.
Does not prove upstream caller, SIM policy, registration, or modem capability.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

TEL_SHA = "63a3bdcf0b681991efae19cec87583e33423d0623ae7386510f6485815c88f52"
TEL_SIZE = 1458120
BASE = 0x100000
# Entry at 0x1f5360; ARM calling convention r0=this, r1/r2/r3 arguments.
BRIDGE = {
    0x1F5360: 0xE92D45F0,  # push
    0x1F536C: 0xE1A08002,  # mov r8, r2: numProvider
    0x1F5370: 0xE1A0A003,  # mov r10, r3: mode
    0x1F538C: 0xE1A07001,  # mov r7, r1: manual
    0x1F53F0: 0xE1A01007,  # mov r1, r7
    0x1F53F4: 0xE1A02008,  # mov r2, r8
    0x1F53F8: 0xE1A0300A,  # mov r3, r10
    0x1F5414: 0xE12FFF3C,  # blx r12 (NAD proxy virtual dispatch)
}

def check_telephone(data):
    identity = len(data) == TEL_SIZE and hashlib.sha256(data).hexdigest() == TEL_SHA
    matches = {}
    for va, w in BRIDGE.items():
        offset = va - BASE
        matches[hex(va)] = (0 <= offset and offset + 4 <= len(data) and
                             struct.unpack_from("<I", data, offset)[0] == w)
    all_match = all(matches.values())
    return {
        "telephone_oem_identity_verified": identity,
        "arm_signature_matches": all_match,
        "arm_checks": matches,
        "select_network_third_arg_forwarded_unchanged":
            True if identity and all_match else None,
        "immediately_upstream_mode_policy_identified": False,
        "sim_specific_mode_value_identified": False,
        "gh_cloud_executed_on_original_elf": False,
        "what_is_proven": "telephone r3 mode forwarded as r3 into NAD proxy method (virtual call).",
        "what_is_not_proven": "caller supplied 1, operator-specific policy, physical LTE registration.",
    }

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("telephone", type=Path)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    res = check_telephone(args.telephone.read_bytes())
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(res, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("TELEPHONE_BRIDGE:", res["select_network_third_arg_forwarded_unchanged"])
    return 0 if res["telephone_oem_identity_verified"] and res["arm_signature_matches"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
