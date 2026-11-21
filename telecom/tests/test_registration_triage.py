import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from registration_triage import triage


class RegistrationTriageTests(unittest.TestCase):
    def test_network_visible_is_not_registered(self):
        x = triage('AT+COPS=?\n+COPS: (1,"CHN-CT","CT","46003",7)\nOK')
        self.assertTrue(x["operator_scan_rows_seen"])
        self.assertEqual(x["lte_verdict"], "NO_UNAMBIGUOUS_LTE_REGISTRATION_EVIDENCE")
        self.assertFalse(x["current_live_state_confirmed"])

    def test_correlated_cereg_registered_home(self):
        x = triage('AT+CEREG?\n+CEREG: 2,1,"1A2B","0000AABB",7\nOK')
        self.assertEqual(x["last_registration_states"]["CEREG"], "REGISTERED_HOME")
        self.assertEqual(x["lte_verdict"], "LTE_REGISTERED_AT_LOG_TIME_ONLY")
        self.assertFalse(x["current_live_state_confirmed"])

    def test_correlated_cereg_denied(self):
        x = triage('AT+CEREG?\n+CEREG: 2,3\nOK')
        self.assertEqual(x["lte_verdict"], "LTE_REGISTRATION_DENIED_AT_LOG_TIME")

    def test_unattributed_two_value_response_is_ambiguous(self):
        x = triage('+CEREG: 2,3')
        self.assertIsNone(x["last_registration_states"]["CEREG"])
        self.assertEqual(x["ambiguous_registration_responses"], 1)
        self.assertEqual(x["lte_verdict"], "NO_UNAMBIGUOUS_LTE_REGISTRATION_EVIDENCE")

    def test_single_value_urc_is_observation_not_live(self):
        x = triage('2026-11-18 11:23 +CEREG: 3')
        self.assertEqual(x["lte_verdict"], "LTE_REGISTRATION_DENIED_AT_LOG_TIME")
        self.assertFalse(x["current_live_state_confirmed"])

    def test_cops_read_only_not_proof(self):
        x = triage('AT+COPS?\n+COPS: 0,2,"46003",7\nOK')
        self.assertTrue(x["operator_read_response_seen"])
        self.assertEqual(x["lte_verdict"], "NO_UNAMBIGUOUS_LTE_REGISTRATION_EVIDENCE")

    def test_simulated_4g_cinterion_response(self):
        # Response format observed in public iscle/qemu-mhi2 Cinterion model.
        x = triage('AT+CEREG?\n+CEREG: 0,1,"0001","00000001",7\nOK')
        self.assertEqual(x["lte_verdict"], "LTE_REGISTERED_AT_LOG_TIME_ONLY")

    def test_attached_does_not_mean_connected(self):
        x = triage('AT+CGATT?\n+CGATT: 1\nOK')
        self.assertTrue(x["packet_attach_response_seen"])
        self.assertFalse(x["data_context_active_response_seen"])
        self.assertEqual(x["lte_verdict"], "NO_UNAMBIGUOUS_LTE_REGISTRATION_EVIDENCE")

    def test_data_activation_still_not_current_radio_proof(self):
        x = triage('+CGACT: 1,1')
        self.assertTrue(x["data_context_active_response_seen"])
        self.assertFalse(x["current_live_state_confirmed"])

    def test_cme_30_and_vendor_hint_not_turned_into_cause(self):
        x = triage('+CME ERROR: 30\n^REJINFO: <redacted>')
        self.assertTrue(x["network_service_error_seen"])
        self.assertTrue(x["vendor_rejection_hint_seen_unparsed"])
        self.assertIsNone(x["last_registration_states"]["CEREG"])

    def test_no_leak_of_sensitive_identifiers(self):
        src = ('IMEI 123456789012345\nICCID 8986001234500000000\n'
               'AT+CEREG?\n+CEREG: 2,2\nOK')
        x = triage(src)
        self.assertNotIn("123456789012345", str(x))
        self.assertNotIn("8986001234500000000", str(x))
        self.assertTrue(x["no_modem_io"])


if __name__ == "__main__":
    unittest.main()
