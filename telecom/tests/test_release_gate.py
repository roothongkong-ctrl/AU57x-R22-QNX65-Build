import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from release_gate import (
    INSTALL_REQUIREMENTS, LTE_SUCCESS_REQUIREMENTS, evaluate)


class ReleaseGateTests(unittest.TestCase):
    def test_blank_evidence_refuses_install(self):
        r = evaluate({})
        self.assertFalse(r["safe_to_build_vehicle_installer"])
        self.assertFalse(r["safe_to_claim_telecom_lte_works"])
        self.assertEqual(set(r["install_blockers"]), set(INSTALL_REQUIREMENTS))

    def test_source_and_oem_checks_alone_insufficient(self):
        r = evaluate({"real_oem_identity_verified": True,
                      "arm_qnx_abi_verified": True})
        self.assertFalse(r["safe_to_build_vehicle_installer"])
        self.assertIn("telecom_mode_failure_path_verified", r["install_blockers"])

    def test_string_true_does_not_bypass_gate(self):
        r = evaluate({k: "true" for k in INSTALL_REQUIREMENTS})
        self.assertFalse(r["safe_to_build_vehicle_installer"])

    def test_r7_backup_only_insufficient(self):
        r = evaluate({"vehicle_original_backup_exists": True,
                      "restore_is_independently_verified": True})
        self.assertFalse(r["safe_to_build_vehicle_installer"])

    def test_all_install_requirements_allow_test_candidate_only(self):
        evidence = dict.fromkeys(INSTALL_REQUIREMENTS, True)
        r = evaluate(evidence)
        self.assertTrue(r["safe_to_build_vehicle_installer"])
        self.assertFalse(r["safe_to_claim_telecom_lte_works"])

    def test_lte_success_requires_real_on_car_evidence(self):
        evidence = dict.fromkeys(LTE_SUCCESS_REQUIREMENTS, True)
        r = evaluate(evidence)
        self.assertTrue(r["safe_to_build_vehicle_installer"])
        self.assertTrue(r["safe_to_claim_telecom_lte_works"])

    def test_mobile_regression_required_for_live_claim(self):
        evidence = dict.fromkeys(LTE_SUCCESS_REQUIREMENTS, True)
        evidence["live_china_mobile_regression_passed"] = False
        r = evaluate(evidence)
        self.assertTrue(r["safe_to_build_vehicle_installer"])
        self.assertFalse(r["safe_to_claim_telecom_lte_works"])

    def test_invalid_evidence_type(self):
        with self.assertRaises(TypeError):
            evaluate("done")


if __name__ == "__main__":
    unittest.main()
