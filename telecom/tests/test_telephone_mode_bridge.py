import hashlib
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from telephone_mode_bridge import BASE, BRIDGE, TEL_SIZE, check_telephone


class TelephoneBridgeTests(unittest.TestCase):
    def synthetic(self):
        b = bytearray(TEL_SIZE)
        for va, w in BRIDGE.items():
            struct.pack_into("<I", b, va - BASE, w)
        return b

    def test_all_eight_bridge_instructions(self):
        self.assertEqual(len(BRIDGE), 8)
        self.assertEqual(BRIDGE[0x1F5370], 0xE1A0A003)
        self.assertEqual(BRIDGE[0x1F53F8], 0xE1A0300A)

    def test_synthetic_never_authenticates_oem_file(self):
        r = check_telephone(self.synthetic())
        self.assertFalse(r["telephone_oem_identity_verified"])
        self.assertTrue(r["arm_signature_matches"])
        self.assertIsNone(r["select_network_third_arg_forwarded_unchanged"])
        self.assertFalse(r["immediately_upstream_mode_policy_identified"])

    def test_modified_forwarding_fails(self):
        b = self.synthetic()
        struct.pack_into("<I", b, 0x1F53F8 - BASE, 0)
        r = check_telephone(b)
        self.assertFalse(r["arm_signature_matches"])
        self.assertFalse(r["arm_checks"]["0x1f53f8"])

    def test_truncated_fails_closed(self):
        r = check_telephone(b"fake")
        self.assertFalse(r["telephone_oem_identity_verified"])
        self.assertFalse(r["arm_signature_matches"])
        self.assertIsNone(r["select_network_third_arg_forwarded_unchanged"])

    def test_output_never_claims_sim_mode_proven(self):
        r = check_telephone(self.synthetic())
        self.assertFalse(r["sim_specific_mode_value_identified"])
        self.assertFalse(r["gh_cloud_executed_on_original_elf"])
        self.assertIn("caller supplied 1", r["what_is_not_proven"])


if __name__ == "__main__":
    unittest.main()
