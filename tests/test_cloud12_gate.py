#!/usr/bin/env python3
"""Positive and mutation-negative tests for an evidence-only audit gate."""
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from sys import path as syspath

ROOT=Path(__file__).resolve().parents[1]
syspath.insert(0,str(ROOT/"tools"))
from cloud12_gate import audit

PATHS=["docs/CLOUD12_LEDGER.json","docs/STEP04_STATIC_EVIDENCE.json",
       "docs/STEP05_MU1005_USB_PLUGIN_ABI_AUDIT.json",
       "docs/R25_GITHUB_REUSE_MATRIX.json"]

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
        self.assertEqual(report["completed_steps"],[1,3])
        self.assertFalse(report["installable"])
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
    def test_truncated_ledger_rejected(self):
        self.change(PATHS[0],lambda x:x["steps"].pop())
        self.assertTrue(audit(self.root)[0])
    def test_false_static_success_rejected(self):
        self.change(PATHS[1],lambda x:x.update(production_wireless_chain_proven=True))
        self.assertTrue(audit(self.root)[0])
if __name__=="__main__":
    unittest.main(verbosity=2)
