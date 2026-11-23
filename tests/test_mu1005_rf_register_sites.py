"""Synthetic ARM images only. No proprietary data enters CI/repository."""
import hashlib
import importlib.util
import pathlib
import struct
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
P = ROOT / "tools" / "mu1005_rf_register_sites.py"
spec = importlib.util.spec_from_file_location("mu1005_rf_register_sites", P)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fake_btstack():
    b = bytearray(0x1bd000)
    b[:16] = b"\x7fELF\x01\x01\x01" + b"\0" * 9
    struct.pack_into("<H", b, 18, 40)
    struct.pack_into("<I", b, 28, 52)
    struct.pack_into("<HH", b, 42, 32, 1)
    struct.pack_into("<IIIIIIII", b, 52, 1, 0, 0x100000, 0, len(b), len(b), 5, 0x1000)
    for call, info in mod.EXPECTED.items():
        disp = (mod.TARGET - call - 8) & 0xffffffff
        assert disp & 3 == 0
        op = 0xeb000000 | ((disp >> 2) & 0xffffff)
        struct.pack_into("<I", b, call - 0x100000, op)
        struct.pack_into("<I", b, info["prep_va"] - 0x100000, info["op"])
    return b


class R89StaticCallers(unittest.TestCase):
    def test_known_two_direct_callers_and_no_iap2_profile(self):
        b = fake_btstack()
        with patch.object(mod, "SHA", hashlib.sha256(b).hexdigest()):
            r = mod.audit(bytes(b))
        self.assertEqual([x["profile"] for x in r["direct_calls"]], [7, 4])
        self.assertFalse(r["iap2_profile_proven"])
        self.assertFalse(r["safe_to_flash"])

    def test_original_hash_guard(self):
        with self.assertRaisesRegex(ValueError, "SHA256"):
            mod.audit(bytes(fake_btstack()))

    def test_changed_original_caller_is_rejected(self):
        b = fake_btstack()
        struct.pack_into("<I", b, 0x2464A0 - 0x100000, 0xe3a02000)
        with patch.object(mod, "SHA", hashlib.sha256(b).hexdigest()):
            with self.assertRaisesRegex(ValueError, "profile"):
                mod.audit(bytes(b))

    def test_unrecognised_third_caller_is_rejected(self):
        b = fake_btstack()
        new = 0x250100
        disp = (mod.TARGET - new - 8) & 0xffffffff
        struct.pack_into("<I", b, new - 0x100000, 0xeb000000 | (disp >> 2 & 0xffffff))
        with patch.object(mod, "SHA", hashlib.sha256(b).hexdigest()):
            with self.assertRaisesRegex(ValueError, "extra"):
                mod.audit(bytes(b))

    def test_invalid_header_rejected_even_if_hash_matches(self):
        b = fake_btstack()
        b[0:4] = b"XXYY"
        with patch.object(mod, "SHA", hashlib.sha256(b).hexdigest()):
            with self.assertRaisesRegex(ValueError, "ELF"):
                mod.audit(bytes(b))

    def test_native_registration_requires_proven_profile(self):
        # R101 moves the external attestation read to au98_publish() and
        # freezes a 1..30 value before entering the native BlueSDK caller.
        # The R89 raw-HFP/SIM static evidence does not prove iAP2 arg3.
        root = ROOT / "project/source/legacy_mu1005/bluetooth"
        src = (root / "native_binding.c").read_text()
        hdr = (root / "native_binding.h").read_text()
        handoff = (root / "bt_iap2_owner_handoff.c").read_text()
        self.assertIn("n.reg=register_verified", src)
        self.assertIn("iap2_profile_proven", hdr)
        self.assertIn("mu_rf_arg_choose(owner->iap2_profile_proven", handoff)
        self.assertIn("s->native_owner.rf_arg3_snapshot=candidate_scn", handoff)
        self.assertIn("r->owner.rf_arg3_snapshot_valid!=1u", src)
        self.assertIn("(channel,slot,r->owner.rf_arg3_snapshot)", src)
        self.assertNotIn("n.reg=RFN(int (*)(void*,uint8_t*,uint16_t),0x277a18u)", src)


if __name__ == "__main__":
    unittest.main()
