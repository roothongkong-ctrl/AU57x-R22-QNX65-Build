#!/usr/bin/env python3
"""Synthetic regression for R99 stock iAP2 config staging; no proprietary blobs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('r99',ROOT/'tools/r99_stock_iap2_route.py')
r99=importlib.util.module_from_spec(spec);spec.loader.exec_module(r99)

BT=json.dumps({'topologyLogic':1,'enableIap':False,'keepFeature':{'carplay':1}}).encode()
INI=b'[general]\nmode=usb\n\n[bluetooth]\nenable = 0\n\n[wifi]\nenable=0\n'

class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.original_bt=self.root/'bt.json';self.original_bt.write_bytes(BT)
        self.original_ini=self.root/'iap2.cfg';self.original_ini.write_bytes(INI)
        self.bin_bt=self.root/'bt.elf';self.bin_bt.write_bytes(b'fixture')
        self.bin_driver=self.root/'driver.so';self.bin_driver.write_bytes(b'fixture')
        self.out=self.root/'review'

    def go(self):
        def mockcheck(path,expected,markers):return {'sha256':expected,'verified':True}
        with patch.object(r99,'audit_binary',side_effect=mockcheck):
            return r99.prepare(self.original_bt,self.original_ini,r99.sha(self.original_bt.read_bytes()),
                         r99.sha(self.original_ini.read_bytes()),self.bin_bt,self.bin_driver,self.out)

    def test_real_pair_staging_and_byte_exact_rollback(self):
        manifest=self.go()
        self.assertFalse(manifest['installable'])
        self.assertFalse(manifest['stock_runtime_iap2_registration_proven'])
        self.assertEqual((self.out/'bluetooth.original.json').read_bytes(),BT)
        self.assertEqual((self.out/'iap2.original.cfg').read_bytes(),INI)
        self.assertTrue(json.loads((self.out/'bluetooth.STAGED_REVIEW_ONLY.json').read_bytes())['enableIap'])
        staged=(self.out/'iap2.STAGED_REVIEW_ONLY.cfg').read_bytes()
        self.assertEqual(staged,INI)
        self.assertEqual(manifest['config_policy'],'BT_JSON_ONLY_PRESERVE_IAP2_V2')
        self.assertFalse(manifest['changes']['iap2_driver']['modified'])
        self.assertIn(b'[wifi]\nenable=0',staged)
        for name,digest in manifest['files'].items():
            self.assertEqual(r99.sha((self.out/name).read_bytes()),digest)
        for name in manifest['files']:
            self.assertEqual((self.out/name).stat().st_mode&0o777,0o600)

    def test_no_bluetooth_section_invented(self):
        raw=b'[general]\r\nmode=usb\r\n'
        self.original_ini.write_bytes(raw)
        self.go()
        self.assertEqual((self.out/'iap2.STAGED_REVIEW_ONLY.cfg').read_bytes(),raw)

    def test_no_overwrite(self):
        self.go()
        with self.assertRaisesRegex(ValueError,'exists'):self.go()

    def test_sha_mismatch_fails_before_write(self):
        with patch.object(r99,'audit_binary',side_effect=RuntimeError('should not reach')):
            with self.assertRaisesRegex(ValueError,'hash mismatch'):
                r99.prepare(self.original_bt,self.original_ini,'0'*64,r99.sha(INI),self.bin_bt,self.bin_driver,self.out)
        self.assertFalse(self.out.exists())

    def test_reject_missing_iap_enable(self):
        with self.assertRaisesRegex(ValueError,'unknown or ambiguous'):
            r99.stage_bluetooth_json(b'{"topologyLogic":1}')

    def test_accept_top_level_dotted(self):
        updated,_=r99.stage_bluetooth_json(b'{"bluetooth.enableIap":false,"keep":1}')
        self.assertTrue(json.loads(updated)['bluetooth.enableIap'])

    def test_accept_nested(self):
        updated,_=r99.stage_bluetooth_json(b'{"bluetooth":{"enableIap":false,"keep":2},"keep":1}')
        self.assertTrue(json.loads(updated)['bluetooth']['enableIap'])

    def test_reject_duplicate_root_and_nested(self):
        with self.assertRaisesRegex(ValueError,'ambiguous'):
            r99.stage_bluetooth_json(b'{"bluetooth.enableIap":false,"bluetooth":{"enableIap":false}}')

    def test_reject_duplicate_json_keys(self):
        with self.assertRaisesRegex(ValueError,'duplicate JSON key'):
            r99.stage_bluetooth_json(b'{"topologyLogic":1,"enableIap":false,"enableIap":true}')

    def test_reject_wrong_json_type(self):
        with self.assertRaisesRegex(ValueError,'must be JSON boolean'):
            r99.stage_bluetooth_json(b'{"topologyLogic":1,"enableIap":"0"}')

    def test_reject_already_enabled(self):
        with self.assertRaisesRegex(ValueError,'already true'):
            r99.stage_bluetooth_json(b'{"topologyLogic":1,"enableIap":true}')

    def test_active_bluetooth_ini_preserves_wifi(self):
        changed,details=r99.stage_iap2_ini(INI)
        self.assertEqual(details['style'],'existing active section')
        self.assertIn(b'[wifi]\nenable=0',changed)

    def test_commented_template_enables_new_section(self):
        raw=b'# [bluetooth]\n# enable=0\n[wifi]\nenable=0\n'
        with self.assertRaisesRegex(ValueError,'no unique'):r99.stage_iap2_ini(raw)
        raw=b'#[bluetooth]\n#enable=0\n[wifi]\nenable=0\n'
        staged,details=r99.stage_iap2_ini(raw)
        self.assertTrue(staged.startswith(raw))
        self.assertIn(b'\n[bluetooth]\nenable = 1\n',staged)
        self.assertIn('commented',details['style'])

    def test_reject_ambiguous_sections(self):
        with self.assertRaisesRegex(ValueError,'duplicate'):
            r99.stage_iap2_ini(b'[bluetooth]\nenable=0\n[bluetooth]\nenable=0\n')

    def test_reject_unknown_ini_value(self):
        with self.assertRaisesRegex(ValueError,'unknown or already true'):
            r99.stage_iap2_ini(b'[bluetooth]\nenable=maybe\n')

    def test_reject_symlink_original(self):
        link=self.root/'other.json';link.symlink_to(self.original_bt)
        with self.assertRaisesRegex(ValueError,'not a regular'):
            r99.read_bounded(link)

    def test_reject_original_oem_hash_wrong(self):
        with self.assertRaisesRegex(ValueError,'incorrect original'):
            r99.audit_binary(self.bin_bt,r99.BLUETOOTH_SHA,r99.OEM_STRING_MARKERS['bluetooth'])

    def test_original_binary_markers_pinned(self):
        # ELF32 ARM layout and all markers must agree with exact, unhashed OEM.
        # Synthesized binary can exercise marker gate only after fixed hash patch.
        fake=b'\x7fELF\x01\x01'+b'\x00'*12+b'\x28\x00'+b'A'*50
        self.bin_bt.write_bytes(fake)
        with patch.object(r99,'sha',return_value=r99.BLUETOOTH_SHA):
            with self.assertRaisesRegex(ValueError,'stock evidence missing'):
                r99.audit_binary(self.bin_bt,r99.BLUETOOTH_SHA,r99.OEM_STRING_MARKERS['bluetooth'])

if __name__=='__main__':unittest.main(verbosity=2)
