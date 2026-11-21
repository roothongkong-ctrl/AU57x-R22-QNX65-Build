#!/usr/bin/env python3
"""Offline, identity-gated NADUtil modem-family evidence.

This inspect-only module NEVER executes NADUtil: the OEM utility itself
initializes/locks serial AT ports, so must not be used as a live read-only probe.
"""
import argparse
import hashlib
import json
from pathlib import Path

EXPECTED_SHA = "67755ff17740728290ce758e80d57904cb6b105bbf4f780d803e97ab5f60f1d2"
EXPECTED_SIZE = 19296
MARKERS = {
    "huawei_me909au_821_literal": (0x3AB0, b"ME909Au-821\x00"),
    "huawei_ati1_query_literal": (0x3A08, b"ATI1\r"),
    "huawei_ati51_query_literal": (0x3D68, b"ATI51\r"),
    "huawei_software_switch_marker": (0x37B4, b"/ramdisk/huawei_me909au_821\x00"),
    "cinterion_als6a_model_literal": (0x3FF8, b"ALS6A\x00"),
    "cinterion_ah6a_model_literal": (0x429C, b"AH6A\x00"),
    "cinterion_software_switch_marker": (0x42C8, b"/ramdisk/cinterion\x00"),
    "huawei_serial_lock_message": (0x3948, b"NADUtil:%d(E) ATPort /dev/serusb3 already locked\x00"),
    "cinterion_serial_port_hint": (0x421C, b"/dev/serusb4\x00"),
}

def check(data):
    identity = len(data) == EXPECTED_SIZE and hashlib.sha256(data).hexdigest() == EXPECTED_SHA
    verified = {
        k: data[offset:offset + len(marker)] == marker
        for k, (offset, marker) in MARKERS.items()
    }
    return {
        "scope": "OFFLINE_ORIGINAL_ELF_LITERAL_INSPECTION",
        "original_nadutil_identity_verified": identity,
        "literal_checks_passed": len(verified) if all(verified.values()) else sum(verified.values()),
        "literal_checks_total": len(verified),
        "literal_results": verified,
        "factory_software_knows_huawei_me909au_821":
            True if identity and all(verified.values()) else None,
        "factory_software_knows_cinterion_als6a_ah6a":
            True if identity and all(verified.values()) else None,
        "actual_vehicle_modem_is_huawei": None,
        "actual_vehicle_modem_is_cinterion": None,
        "serial_or_modem_was_accessed": False,
        "original_nadutil_executed": False,
        "need_next": "Obtain already-owned NAD driver/log family-selection evidence; do NOT launch NADUtil or seize /dev/serusb3/4 while the original NAD is active.",
    }

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("nadutil", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    r = check(a.nadutil.read_bytes())
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(r, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("OEM_NADUTIL_VERIFIED=",r["original_nadutil_identity_verified"],
          "MODEM_FAMILY_ON_CAR=UNKNOWN")
    return 0 if r["original_nadutil_identity_verified"] and r["literal_checks_passed"] == r["literal_checks_total"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
