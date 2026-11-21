#!/usr/bin/env python3
"""Original AU57x NAD byte-level signature gate (read-only).

The offset values were verified against the user's original OEM sample in
2026-11-22 offline_tests.py; this gate does NOT establish the runtime mode.
Never commit the original NAD ELF to a public repository.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED_SIZE = 845158
EXPECTED_SHA256 = "ed7202ec171deb978f07d71efcad836707be3cb9a1104ced98c51709649d3e21"
ELF_VA_MINUS_FILE_OFFSET = 0x100000

ARM_WORDS = {
    0x18B160: 0xE5951010,  # ldr r1,[r5,#16]
    0x18B168: 0xE251B001,  # subs r11,r1,#1
    0x18B16C: 0x13A0B001,  # movne r11,#1
    0x18B1A4: 0xE35B0000,  # cmp r11,#0 (manual)
    0x18B1A8: 0x0A0000FA,  # beq -> GSM manual
    0x18B248: 0xE35B0000,  # cmp r11,#0 (auto)
    0x18B24C: 0x1A0000C5,  # bne -> auto non-GSM
    0x18B1CC: 0xE59F3760,  # ldr normal manual template
    0x18B5B8: 0xE59F3398,  # ldr GSM manual template
    0x18B26C: 0xE59F36C8,  # ldr GSM automatic template
    0x18B584: 0xE59F33C8,  # ldr normal automatic template
}
AT_LITERALS = {
    0xC6488: b"AT+COPS=1,2,%s\x00",
    0xC6498: b"AT+COPS=1,2,%s,0\x00",
    0xC64AC: b"AT+COPS=0,2,,0\x00",
    0xC19C0: b"AT+COPS=0,2\x00",
}


def verify_code_bytes(raw):
    """Check recorded code/literals, not full identity or runtime execution."""
    failed = []
    for va, expected in ARM_WORDS.items():
        pos = va - ELF_VA_MINUS_FILE_OFFSET
        if pos < 0 or pos + 4 > len(raw) or struct.unpack_from("<I", raw, pos)[0] != expected:
            failed.append("ARM@" + hex(va))
    for pos, expected in AT_LITERALS.items():
        if raw[pos:pos + len(expected)] != expected:
            failed.append("AT_LITERAL@" + hex(pos))
    return failed


def inspect_nad(raw):
    identity = len(raw) == EXPECTED_SIZE and (
        hashlib.sha256(raw).hexdigest() == EXPECTED_SHA256
    )
    # Identity-gated; an unrelated build must never pass this report.
    failed = verify_code_bytes(raw) if identity else ["IDENTITY_MISMATCH"]
    return {
        "sample_identity_verified": identity,
        "code_literal_checks_total": len(ARM_WORDS) + len(AT_LITERALS),
        "code_literal_checks_passed": identity and not failed,
        "failed_checks": failed,
        "runtime_mode_observed": False,
        "mode_writer_identified": False,
        "sim_registered_on_lte": False,
        "safe_to_install_patch": False,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("nad", type=Path, help="Local owner-authorized original nad ELF")
    p.add_argument("--json", action="store_true")
    a = p.parse_args()
    result = inspect_nad(a.nad.read_bytes())
    if a.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print("OEM identity: %s; ARM+AT signature: %s; runtime mode: UNKNOWN"
              % (result["sample_identity_verified"],
                 result["code_literal_checks_passed"]))
    return 0 if result["code_literal_checks_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
