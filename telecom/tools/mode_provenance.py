#!/usr/bin/env python3
"""AU57x OEM mode parameter provenance: exact ARM callsite inspection.

Locations were derived from owner-authorized nad ELF read-only reverse
engineering. No firmware image is committed. No AT write, no patch.

The trace reaches the NAD request structure, NOT the SIM policy that set it.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED_SIZE = 845158
EXPECTED_SHA = "ed7202ec171deb978f07d71efcad836707be3cb9a1104ced98c51709649d3e21"
SEGMENT_BASE = 0x100000
NET_ROUTINE = 0x18B0EC
MOVE_REQ_TO_R5 = (0x18B104, 0xE1A05001)  # mov r5, r1 (input event pointer)
LOAD_MODE_FIELD = (0x18B160, 0xE5951010)  # ldr r1, [r5,#0x10]
CMP_MODE_1 = (0x18B168, 0xE251B001)
CALLERS = {
    0x14CD3C: 0xEB00F8EA,
    0x15625C: 0xEB00D3A2,
    0x1731C4: 0xEB005FC8,
}


def word_at(data, va):
    p = va - SEGMENT_BASE
    if p < 0 or p + 4 > len(data):
        raise ValueError("VA_NOT_MAPPED")
    return struct.unpack_from("<I", data, p)[0]


def direct_arm_bl_target(va, arm_word):
    """Decode unconditional ARM-state BL pc-relative target."""
    if arm_word & 0xFF000000 != 0xEB000000:
        raise ValueError("NOT_UNCONDITIONAL_ARM_BL")
    signed_imm24 = arm_word & 0xFFFFFF
    if signed_imm24 & 0x800000:
        signed_imm24 -= 0x1000000
    return va + 8 + signed_imm24 * 4


def verify_mode_path(data):
    identity = len(data) == EXPECTED_SIZE and hashlib.sha256(data).hexdigest() == EXPECTED_SHA
    signatures = [MOVE_REQ_TO_R5, LOAD_MODE_FIELD, CMP_MODE_1, *CALLERS.items()]
    matches = {hex(va): word_at(data, va) == expected for va, expected in signatures}
    targets = {}
    for call_va, expected in CALLERS.items():
        if word_at(data, call_va) == expected:
            targets[hex(call_va)] = hex(direct_arm_bl_target(call_va, expected))
        else:
            targets[hex(call_va)] = "SIGNATURE_MISMATCH"
    return {
        "original_nad_identity_verified": identity,
        "path_arm_instructions_match": all(matches.values()),
        "exact_signatures": matches,
        "direct_caller_targets": targets,
        "input_pointer_arrives_in_r1": True if identity and all(matches.values()) else None,
        "nad_reads_mode_at_request_plus_0x10": True if identity and all(matches.values()) else None,
        "upstream_mode_assignment_identified": False,
        "sim_imsi_to_mode_policy_identified": False,
        "lte_registration_of_telecom_proven": False,
        "safe_to_generate_install_patch": False,
        "confidence": "EXACT_OEM_MACHINE_CODE_ONLY" if identity and all(matches.values()) else "UNVERIFIED",
        "need_next": "Disassemble caller/request deserializer and telephone NADServicesProxy; correlate read-only runtime mode & LTE state.",
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("nad", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    result = verify_mode_path(a.nad.read_bytes())
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(result["confidence"], "mode assignment=", result["upstream_mode_assignment_identified"])
    return 0 if result["original_nad_identity_verified"] and result["path_arm_instructions_match"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
