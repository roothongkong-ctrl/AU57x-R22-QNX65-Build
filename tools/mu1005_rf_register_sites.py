#!/usr/bin/env python3
"""Read-only, exact-SHA ARM direct-BL callsite audit on user's private MU1005 btstack.
Only metadata/addresses published. Never copies OEM code or emits OEM bytes.
"""
import argparse
import hashlib
import json
import pathlib
import re
import struct

SHA = "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
TARGET = 0x277A18
EXPECTED = {
    0x2464B0: {"profile": 7, "service": "HFP", "prep_va": 0x2464A0, "op": 0xE3A02007},
    0x24DCAC: {"profile": 4, "service": "SIM", "prep_va": 0x24DCA8, "op": 0xE3A02004},
}


def audit(blob: bytes) -> dict:
    digest = hashlib.sha256(blob).hexdigest()
    if digest != SHA:
        raise ValueError("MU1005 exact OEM btstack SHA256 mismatch")
    if len(blob) < 52 or blob[:16] != b"\x7fELF\x01\x01\x01" + b"\0" * 9:
        raise ValueError("not exact ELF32 little-endian OEM image")
    if struct.unpack_from("<H", blob, 18)[0] != 40:
        raise ValueError("not ARM")
    phoff = struct.unpack_from("<I", blob, 28)[0]
    phsize, count = struct.unpack_from("<HH", blob, 42)
    if phsize < 32 or count < 1 or phoff + phsize * count > len(blob):
        raise ValueError("invalid program headers")
    words = {}
    for i in range(count):
        ptype, fileoff, va, _, size, _, flags, _ = struct.unpack_from("<IIIIIIII", blob, phoff + phsize * i)
        if ptype != 1 or not (flags & 1):
            continue
        if fileoff + size > len(blob):
            raise ValueError("executable segment truncated")
        for at in range(fileoff, fileoff + size - 3, 4):
            words[va + at - fileoff] = struct.unpack_from("<I", blob, at)[0]
    calls = []
    for va, op in words.items():
        if (op & 0x0F000000) != 0x0B000000:
            continue
        displacement = (op & 0x00FFFFFF) << 2
        if displacement & (1 << 25):
            displacement -= (1 << 26)
        if va + 8 + displacement == TARGET:
            if va not in EXPECTED:
                raise ValueError("unknown extra direct RF register caller")
            exp = EXPECTED[va]
            if words.get(exp["prep_va"]) != exp["op"]:
                raise ValueError("stock RF registration profile register differs")
            calls.append({"call_va": f"0x{va:08x}",
                          "profile": exp["profile"], "service": exp["service"],
                          "source": "original ARM mov r2 immediate before BL"})
    if len(calls) != len(EXPECTED):
        raise ValueError("missing known direct RF register caller")
    return {
        "firmware": "MHI2_CN_AU57x_K1006_2/MU1005",
        "binary": "btstack", "sha256": digest,
        "rf_register_va": f"0x{TARGET:08x}",
        "direct_calls": sorted(calls, key=lambda a: a["call_va"]),
        "important": "Third argument varies by original service (HFP 7, SIM 4); no original iAP2 third argument established.",
        "iap2_profile_proven": False,
        "native_injection_proven": False,
        "actual_iphone_connected": False,
        "vehicle_verified": False,
        "safe_to_flash": False,
    }


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--btstack", required=True)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    result = audit(pathlib.Path(args.btstack).read_bytes())
    pathlib.Path(args.out).write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("MU1005 original RF registration direct callsites: 2/2; iAP2 mode NOT VERIFIED")


if __name__ == "__main__":
    main()