import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from transaction_sim import (
    TransactionError, recover_pending, sha, simulate_power_loss, simulate_update)


class TransactionSimulationTests(unittest.TestCase):
    def fixture(self, root):
        t, c, b = root / "factory.bin", root / "candidate.bin", root / "backup.bin"
        t.write_bytes(b"FACTORY_APN_OR_NAD_SAMPLE")
        c.write_bytes(b"CARRIER_SCOPED_TEST_CANDIDATE")
        return t, c, b

    def call(self, t, c, b, fault=None):
        return simulate_update(
            t, c, b,
            expected_original_sha=sha(b"FACTORY_APN_OR_NAD_SAMPLE"),
            expected_candidate_sha=sha(b"CARRIER_SCOPED_TEST_CANDIDATE"),
            fault=fault)

    def test_simulated_success(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            r = self.call(t, c, b)
            self.assertEqual(r["result"], "SIMULATED_APPLY_OK")
            self.assertEqual(t.read_bytes(), c.read_bytes())
            self.assertEqual(b.read_bytes(), b"FACTORY_APN_OR_NAD_SAMPLE")
            self.assertFalse(t.with_name(t.name + ".au57x.pending").exists())

    def test_fault_after_backup_keeps_original(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            with self.assertRaisesRegex(TransactionError, "FAULT_AFTER_BACKUP"):
                self.call(t, c, b, "after_backup")
            self.assertEqual(t.read_bytes(), b"FACTORY_APN_OR_NAD_SAMPLE")

    def test_faults_rollback_after_any_commit_point(self):
        for injection in ("after_stage", "after_journal",
                          "after_replace", "after_verify"):
            with self.subTest(injection=injection), tempfile.TemporaryDirectory() as d:
                t, c, b = self.fixture(Path(d))
                with self.assertRaises(TransactionError):
                    self.call(t, c, b, injection)
                self.assertEqual(t.read_bytes(), b"FACTORY_APN_OR_NAD_SAMPLE")
                self.assertFalse(t.with_name(t.name + ".au57x.pending").exists())
                self.assertFalse(t.with_name(t.name + ".au57x.stage").exists())

    def test_mismatch_rejected_before_any_write(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            t.write_bytes(b"UNIDENTIFIED_FIRMWARE")
            with self.assertRaisesRegex(TransactionError, "ORIGINAL_IDENTITY_MISMATCH"):
                self.call(t, c, b)
            self.assertFalse(b.exists())
            self.assertEqual(t.read_bytes(), b"UNIDENTIFIED_FIRMWARE")

    def test_candidate_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            c.write_bytes(b"CORRUPTED_PATCH")
            with self.assertRaisesRegex(TransactionError, "CANDIDATE_INVALID"):
                self.call(t, c, b)
            self.assertFalse(b.exists())

    def test_invalid_backup_rejected_without_modification(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            b.write_bytes(b"UNTRUSTED")
            with self.assertRaisesRegex(TransactionError, "BACKUP_IDENTITY_MISMATCH"):
                self.call(t, c, b)
            self.assertEqual(t.read_bytes(), b"FACTORY_APN_OR_NAD_SAMPLE")

    def test_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            t, c, b = self.fixture(root)
            outside = root / "outside"
            outside.write_bytes(b"keep")
            b.symlink_to(outside)
            with self.assertRaisesRegex(TransactionError, "REFUSE_SYMLINK"):
                self.call(t, c, b)
            self.assertEqual(outside.read_bytes(), b"keep")

    def test_pending_journal_refused_until_recovered(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            pending = t.with_name(t.name + ".au57x.pending")
            pending.write_text('{"factory_sha256":"x"}')
            with self.assertRaisesRegex(TransactionError, "PREVIOUS_TRANSACTION_MUST_BE_RECOVERED"):
                self.call(t, c, b)
            self.assertEqual(t.read_bytes(), b"FACTORY_APN_OR_NAD_SAMPLE")

    def test_simulated_power_loss_manual_recovery(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            old_sha = sha(t.read_bytes())
            new_sha = sha(c.read_bytes())
            self.assertEqual(
                simulate_power_loss(t, c, b, original_sha=old_sha, candidate_sha=new_sha),
                "SIMULATED_UNCLEAN_SHUTDOWN")
            self.assertTrue(t.with_name(t.name + ".au57x.pending").exists())
            self.assertEqual(t.read_bytes(), b"CARRIER_SCOPED_TEST_CANDIDATE")
            self.assertEqual(recover_pending(t, b, old_sha), "SIMULATED_RECOVERY_OK")
            self.assertEqual(t.read_bytes(), b"FACTORY_APN_OR_NAD_SAMPLE")
            self.assertFalse(t.with_name(t.name + ".au57x.pending").exists())

    def test_wrong_recovery_identity_refused(self):
        with tempfile.TemporaryDirectory() as d:
            t, c, b = self.fixture(Path(d))
            old_sha, new_sha = sha(t.read_bytes()), sha(c.read_bytes())
            simulate_power_loss(t, c, b, original_sha=old_sha, candidate_sha=new_sha)
            with self.assertRaisesRegex(TransactionError, "JOURNAL_ORIGINAL_ID_MISMATCH"):
                recover_pending(t, b, "wrong")
            self.assertEqual(t.read_bytes(), b"CARRIER_SCOPED_TEST_CANDIDATE")


if __name__ == "__main__":
    unittest.main()
