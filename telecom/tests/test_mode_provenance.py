import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from mode_provenance import (CALLERS, CMP_MODE_1, EXPECTED_SIZE,
    LOAD_MODE_FIELD, MOVE_REQ_TO_R5, NET_ROUTINE,
    SEGMENT_BASE, direct_arm_bl_target, verify_mode_path, word_at)


def fixture():
    # Synthetic buffer with known ARM signatures, NOT a proprietary binary.
    buf = bytearray(EXPECTED_SIZE)
    for va, w in [MOVE_REQ_TO_R5, LOAD_MODE_FIELD, CMP_MODE_1, *CALLERS.items()]:
        struct.pack_into("<I", buf, va - SEGMENT_BASE, w)
    return buf


class ProvenanceTests(unittest.TestCase):
    def test_three_direct_oem_calls(self):
        self.assertEqual(len(CALLERS), 3)
        for va, instruction in CALLERS.items():
            self.assertEqual(direct_arm_bl_target(va, instruction), NET_ROUTINE)

    def test_mode_field_exact_register_and_offset(self):
        b = fixture()
        self.assertEqual(word_at(b, MOVE_REQ_TO_R5[0]), 0xE1A05001)
        self.assertEqual(word_at(b, LOAD_MODE_FIELD[0]), 0xE5951010)

    def test_wrong_arm_branch_rejected(self):
        with self.assertRaises(ValueError):
            direct_arm_bl_target(0x14CD3C, 0xE1A05001)

    def test_unmapped_va_rejected(self):
        with self.assertRaises(ValueError):
            word_at(b"\x00\x00", NET_ROUTINE)

    def test_synthetic_binary_never_claims_true_original(self):
        r = verify_mode_path(fixture())
        self.assertFalse(r["original_nad_identity_verified"])
        self.assertTrue(r["path_arm_instructions_match"])
        self.assertIsNone(r["nad_reads_mode_at_request_plus_0x10"])
        self.assertFalse(r["upstream_mode_assignment_identified"])
        self.assertFalse(r["safe_to_generate_install_patch"])

    def test_changed_caller_detected(self):
        b = fixture()
        struct.pack_into("<I", b, 0x1731C4 - SEGMENT_BASE, 0)
        r = verify_mode_path(b)
        self.assertFalse(r["path_arm_instructions_match"])
        self.assertEqual(r["direct_caller_targets"]["0x1731c4"], "SIGNATURE_MISMATCH")


if __name__ == "__main__":
    unittest.main()
