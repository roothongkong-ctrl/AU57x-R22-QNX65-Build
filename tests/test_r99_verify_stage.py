import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import r99_stock_iap2_route as route
import r99_verify_stage as verify_mod

class Verify(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.bt=self.root/'bt.json'
        self.bt.write_text('{"topologyLogic":1,"enableIap":false,"setting":2}\n')
        self.cfg=self.root/'iap2.cfg';self.cfg.write_text('[bluetooth]\nenable=0\n')
        self.staged=self.root/'stage'
        with patch.object(route,'audit_binary',side_effect=lambda a,b,c:{'sha256':b,'verified':True}):
            route.prepare(self.bt,self.cfg,route.sha(self.bt.read_bytes()),route.sha(self.cfg.read_bytes()),
                          self.root/'bt.elf',self.root/'driver.elf',self.staged)
    def test_valid(self):
        x=verify_mod.verify(self.staged,route.sha(self.bt.read_bytes()),route.sha(self.cfg.read_bytes()))
        self.assertTrue(x['R99_STAGE_VERIFIED'])
    def test_tamper_stage_fail(self):
        p=self.staged/'iap2.STAGED_REVIEW_ONLY.cfg';p.write_bytes(p.read_bytes()+b'junk')
        with self.assertRaisesRegex(ValueError,'SHA256 mismatch'):verify_mod.verify(self.staged)
    def test_external_hash_required_match(self):
        with self.assertRaisesRegex(ValueError,'external Bluetooth'):
            verify_mod.verify(self.staged,'0'*64)
    def test_no_extra_payload(self):
        (self.staged/'btstack').write_bytes(b'hello')
        with self.assertRaisesRegex(ValueError,'content names mismatch'):verify_mod.verify(self.staged)
    def test_symlink_refused(self):
        p=self.staged/'iap2.STAGED_REVIEW_ONLY.cfg';p.unlink();p.symlink_to(self.cfg)
        with self.assertRaisesRegex(ValueError,'regular file'):verify_mod.verify(self.staged)
    def test_forged_installable_false_required(self):
        p=self.staged/'R99_REVIEW_MANIFEST.json'
        import json
        j=json.loads(p.read_text());j['installable']=True;p.write_text(json.dumps(j))
        with self.assertRaisesRegex(ValueError,'explicitly false'):verify_mod.verify(self.staged)

if __name__=='__main__':unittest.main()
