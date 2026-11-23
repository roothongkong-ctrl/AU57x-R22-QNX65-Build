import json
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/'tools'))
from r94_launcher_patch import stage, STOCK

class R97HfpLauncher(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.p=Path(self.tmp.name)
        self.raw=json.dumps({'modules':[{'name':'btstack','exec':STOCK,
                                          'preCondition':'/tmp/mvloaded',
                                          'flags':['silent','auto']},
                                         {'name':'bluetooth','exec':'/eso/bin/apps/bluetooth'}]},sort_keys=True).encode()
    def tearDown(self):self.tmp.cleanup()
    def test_explicit_opt_in(self):
        r=stage(self.raw,self.p/'yes',hfp_probe=True)
        shell=(self.p/'yes'/'au57x_btstack_probe_entry.sh').read_text()
        self.assertTrue(r['hfp_got_observer_opt_in'])
        self.assertIn('AU57X_R97_GOT_PROBE=1',shell)
        modified=json.loads((self.p/'yes'/'connectivity.STAGED_REVIEW_ONLY.json').read_text())
        self.assertEqual(modified['modules'][0]['preCondition'],'/tmp/mvloaded')
        self.assertEqual(modified['modules'][0]['flags'],['silent','auto'])
        self.assertEqual(modified['modules'][1]['exec'],'/eso/bin/apps/bluetooth')
        self.assertEqual((self.p/'yes'/'connectivity.original.json').read_bytes(),self.raw)
        self.assertFalse(r['vehicle_verified'])
    def test_default_off(self):
        r=stage(self.raw,self.p/'no')
        self.assertFalse(r['hfp_got_observer_opt_in'])
        self.assertNotIn('AU57X_R97_GOT_PROBE',
                         (self.p/'no'/'au57x_btstack_probe_entry.sh').read_text())
    def test_duplicate_stock_process_rejected(self):
        d=json.loads(self.raw)
        d['modules'].append({'name':'btstack','exec':STOCK})
        with self.assertRaises(ValueError):stage(json.dumps(d).encode(),self.p/'dupe',hfp_probe=True)
    def test_bad_opt_in_rejected(self):
        with self.assertRaises(ValueError):stage(self.raw,self.p/'bad',hfp_probe=1)

if __name__=='__main__':unittest.main()
