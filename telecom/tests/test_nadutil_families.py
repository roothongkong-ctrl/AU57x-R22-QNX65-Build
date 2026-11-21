import hashlib
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from nadutil_families import check, MARKERS, EXPECTED_SIZE


def synthetic():
    out = bytearray(EXPECTED_SIZE)
    for off, marker in MARKERS.values():
        out[off:off+len(marker)] = marker
    return out


class NADUtilFamilyTests(unittest.TestCase):
    def test_synthetic_contains_exact_literature_but_cannot_claim_oem(self):
        r=check(synthetic())
        self.assertEqual(r["literal_checks_passed"],len(MARKERS))
        self.assertFalse(r["original_nadutil_identity_verified"])
        self.assertIsNone(r["factory_software_knows_huawei_me909au_821"])
        self.assertIsNone(r["actual_vehicle_modem_is_huawei"])

    def test_exact_ati_requests_include_line_endings(self):
        self.assertEqual(MARKERS["huawei_ati1_query_literal"][1],b"ATI1\r")
        self.assertEqual(MARKERS["huawei_ati51_query_literal"][1],b"ATI51\r")

    def test_wrong_model_marker_rejected(self):
        b=synthetic()
        off, _ = MARKERS["cinterion_als6a_model_literal"]
        b[off]=ord("X")
        r=check(b)
        self.assertFalse(r["literal_results"]["cinterion_als6a_model_literal"])
        self.assertIsNone(r["factory_software_knows_cinterion_als6a_ah6a"])

    def test_short_or_undefined_content_rejected(self):
        r=check(b"")
        self.assertFalse(r["original_nadutil_identity_verified"])
        self.assertFalse(r["literal_checks_passed"])
        self.assertFalse(r["original_nadutil_executed"])

    def test_never_conflates_supported_family_with_installed_hardware(self):
        r=check(synthetic())
        self.assertIsNone(r["actual_vehicle_modem_is_huawei"])
        self.assertIsNone(r["actual_vehicle_modem_is_cinterion"])
        self.assertFalse(r["serial_or_modem_was_accessed"])
        self.assertIn("do NOT launch NADUtil",r["need_next"])


if __name__=="__main__":
    unittest.main()
