import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from oem_signature import (
    ARM_WORDS, AT_LITERALS, EXPECTED_SIZE,
    ELF_VA_MINUS_FILE_OFFSET, inspect_nad, verify_code_bytes,
)


def synthetic_code_fixture():
    # Synthetic bytes only: not an OEM binary, not proof of actual build.
    b = bytearray(EXPECTED_SIZE)
    for va, word in ARM_WORDS.items():
        struct.pack_into("<I", b, va - ELF_VA_MINUS_FILE_OFFSET, word)
    for pos, literal in AT_LITERALS.items():
        b[pos:pos + len(literal)] = literal
    return b


class OriginalSignatureGateTests(unittest.TestCase):
    def test_original_evidence_has_11_arm_words_and_4_templates(self):
        self.assertEqual(len(ARM_WORDS), 11)
        self.assertEqual(len(AT_LITERALS), 4)
        self.assertEqual(ARM_WORDS[0x18B160], 0xE5951010)

    def test_signature_checker_accepts_synthetic_fixture_only_as_code(self):
        fake = synthetic_code_fixture()
        self.assertEqual(verify_code_bytes(fake), [])
        # Exact approved SHA256 gates full OEM identity; fake cannot pass.
        result = inspect_nad(fake)
        self.assertFalse(result["sample_identity_verified"])
        self.assertFalse(result["code_literal_checks_passed"])

    def test_mutated_arm_branch_rejected(self):
        fake = synthetic_code_fixture()
        struct.pack_into("<I", fake, 0x18B1A8 - ELF_VA_MINUS_FILE_OFFSET,
                         0x00000000)
        self.assertIn("ARM@0x18b1a8", verify_code_bytes(fake))

    def test_mutated_gsm_literal_rejected(self):
        fake = synthetic_code_fixture()
        fake[0xC6498] = ord("X")
        self.assertIn("AT_LITERAL@0xc6498", verify_code_bytes(fake))

    def test_truncated_input_fails_closed(self):
        result = inspect_nad(b"\x7fELF")
        self.assertFalse(result["code_literal_checks_passed"])
        self.assertIn("IDENTITY_MISMATCH", result["failed_checks"])
        self.assertFalse(result["safe_to_install_patch"])


if __name__ == "__main__":
    unittest.main()
