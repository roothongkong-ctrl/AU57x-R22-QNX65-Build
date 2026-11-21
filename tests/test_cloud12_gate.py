#!/usr/bin/env python3
"""Positive and mutation-negative tests for an evidence-only audit gate."""
import json
import shutil
import tempfile
import subprocess
from unittest.mock import patch
import unittest
from pathlib import Path
from sys import path as syspath

ROOT=Path(__file__).resolve().parents[1]
syspath.insert(0,str(ROOT/"tools"))
from cloud12_gate import audit

PATHS=["docs/CLOUD12_LEDGER.json","docs/STEP04_STATIC_EVIDENCE.json",
       "docs/STEP05_MU1005_USB_PLUGIN_ABI_AUDIT.json",
       "docs/R25_GITHUB_REUSE_MATRIX.json","docs/R29_BT_DSI_OWNER_AUDIT.json",
       "docs/R31_MM_IPOD_AUDIT.json","docs/R45_MU1005_BTSTACK_STATIC_RECHECK.json"]

class EvidenceGateTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.root=Path(self.tmp.name)
        for rel in PATHS:
            (self.root/rel).parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(ROOT/rel,self.root/rel)
    def tearDown(self):
        self.tmp.cleanup()
    def change(self,rel,fn):
        p=self.root/rel
        obj=json.loads(p.read_text())
        fn(obj)
        p.write_text(json.dumps(obj))
    def test_original_evidence_gate(self):
        errs,report=audit(self.root)
        self.assertFalse(errs,errs)
        self.assertEqual(report["completed_steps"],[1,2,3,10])
        self.assertFalse(report["installable"])
    def test_tracked_file_scan_failure_fails_closed(self):
        (self.root/".git").mkdir()
        with patch("cloud12_gate.subprocess.run",side_effect=subprocess.CalledProcessError(1,"git")):
            errs,_=audit(self.root)
        self.assertTrue(any("Cannot inspect tracked files" in e for e in errs),errs)
    def test_json_only_fixture_skips_git_scan(self):
        with patch("cloud12_gate.subprocess.run",side_effect=OSError("git unavailable")):
            errs,_=audit(self.root)
        self.assertFalse(errs,errs)
    def test_lies_about_installation_rejected(self):
        self.change(PATHS[0],lambda x:x.update(installable=True))
        self.assertTrue(audit(self.root)[0])
    def test_fake_done_step_rejected(self):
        self.change(PATHS[0],lambda x:x["steps"][3].update(status="DONE"))
        self.assertTrue(audit(self.root)[0])
    def test_mfi_claim_rejected(self):
        self.change(PATHS[2],lambda x:x.update(actual_mfi_verified=True))
        self.assertTrue(audit(self.root)[0])
    def test_wrong_r14_digest_rejected(self):
        self.change(PATHS[3],lambda x:x.update(exact_oem_btstack_sha256="0"*64))
        self.assertTrue(audit(self.root)[0])
    def test_fake_high_level_dynamic_owner_rejected(self):
        self.change(PATHS[4],lambda x:x.update(rfcomm_to_iap2_owner_proven=True))
        self.assertTrue(audit(self.root)[0])
    def test_false_ipod_bluetooth_link_rejected(self):
        self.change(PATHS[5],lambda x:x.update(bluetooth_to_mm_ipod_link_proven=True))
        self.assertTrue(audit(self.root)[0])
    def test_r45_false_runtime_owner_rejected(self):
        self.change(PATHS[6],lambda x:x.update(runtime_owner_proven=True))
        self.assertTrue(audit(self.root)[0])
    def test_r45_partial_instruction_sample_rejected(self):
        self.change(PATHS[6],lambda x:x["rows"][0].update(matches=False))
        self.assertTrue(audit(self.root)[0])
    def test_r45_duplicate_address_rejected(self):
        self.change(PATHS[6],lambda x:x["rows"][1].update(address=x["rows"][0]["address"]))
        self.assertTrue(audit(self.root)[0])
    def test_truncated_ledger_rejected(self):
        self.change(PATHS[0],lambda x:x["steps"].pop())
        self.assertTrue(audit(self.root)[0])
    def test_false_static_success_rejected(self):
        self.change(PATHS[1],lambda x:x.update(production_wireless_chain_proven=True))
        self.assertTrue(audit(self.root)[0])
if __name__=="__main__":
    unittest.main(verbosity=2)
