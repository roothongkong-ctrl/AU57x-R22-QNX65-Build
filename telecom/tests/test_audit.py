import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from audit import audit, build_cops, report_md, scenarios


class ModeTemplateTests(unittest.TestCase):
    def test_manual_mode_one_forces_gsm(self):
        self.assertEqual(build_cops(1, "manual", "46003"), "AT+COPS=1,2,46003,0")

    def test_other_modes_do_not_force_gsm(self):
        for mode in (0, 2, 3, 7):
            self.assertEqual(build_cops(mode, "manual", "46003"), "AT+COPS=1,2,46003")

    def test_automatic_templates_are_distinct(self):
        self.assertEqual(build_cops(1, "auto"), "AT+COPS=0,2,,0")
        for mode in (0, 2, 7):
            self.assertEqual(build_cops(mode, "auto"), "AT+COPS=0,2")

    def test_six_cases(self):
        rows = scenarios()
        self.assertEqual(len(rows), 6)
        self.assertEqual(
            len({(r["mode"], r["selection"]) for r in rows}), 6)

    def test_validate_network(self):
        for plmn in ("", "4600", "4600312", "4600x", "46003,7"):
            with self.subTest(plmn=plmn):
                with self.assertRaises(ValueError):
                    build_cops(1, "manual", plmn)
        self.assertEqual(build_cops(2, "manual", "460011"), "AT+COPS=1,2,460011")

    def test_reject_bad_mode_and_selection(self):
        for mode in (True, "1", -1, None):
            with self.subTest(mode=mode):
                with self.assertRaises(ValueError):
                    build_cops(mode, "manual")
        with self.assertRaises(ValueError):
            build_cops(1, "manual-unsafe")


class EvidenceBoundaryTests(unittest.TestCase):
    def test_without_original_files_is_evidence_only(self):
        r = audit()
        self.assertEqual(r["report_status"], "EVIDENCE_ONLY")
        self.assertEqual(r["files"], [])
        self.assertFalse(r["live_modem_test"])
        self.assertFalse(r["installer_produced"])
        self.assertFalse(r["mode_source_confirmed"])

    def test_no_false_live_registration_claim(self):
        r = audit()
        self.assertTrue(r["baseline_observations"]["chn_ct_seen_in_scan"])
        self.assertFalse(r["baseline_observations"]["lte_registration_proven"])
        self.assertFalse(r["baseline_observations"]["runtime_mode_proven"])
        self.assertFalse(r["baseline_observations"]["live_cereg_proven"])

    def test_apn_does_not_claim_registration(self):
        r = audit()
        self.assertEqual(r["apn_original_records"], 874)
        self.assertEqual(len(r["apn_telecom_entries_missing_in_original"]), 4)
        self.assertFalse(
            r["baseline_observations"]["adding_ctnet_alone_resolved_problem"])

    def test_mobile_sim_protected_and_no_flash(self):
        r = audit()
        self.assertTrue(
            r["mobile_sim_compatibility_protected_by_no_firmware_changes"])
        self.assertFalse(r["installer_produced"])
        self.assertNotIn("AT+COPS=1,2,46003,7",
                         [x["command"] for x in r["mode_templates"]])

    def test_report_names_limitations(self):
        text = report_md(audit())
        self.assertIn("EVIDENCE_ONLY", text)
        self.assertIn("mode 参数传递", text)
        self.assertIn("不是实车执行", text)
        self.assertIn("不修改原厂二进制", text)


if __name__ == "__main__":
    unittest.main()
