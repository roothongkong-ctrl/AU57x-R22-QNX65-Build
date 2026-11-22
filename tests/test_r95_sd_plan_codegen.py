import json
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import r95_sd_plan_codegen as codegen

class R95TransactionPlan(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='au57x-r95-')
        self.r=Path(self.temp.name)
        (self.r/codegen.MARKER).write_text('FIXTURE_ONLY\n')
        for d in ('target','backup','payload','state'):(self.r/d).mkdir()
        for name in ('bluetooth.fixture','si.fixture'):
            (self.r/'target'/name).write_bytes(('old-'+name).encode())
            (self.r/'payload'/name).write_bytes(('new-'+name).encode())
        self.o=self.r/'generated'
    def tearDown(self):self.temp.cleanup()
    def build(self):return codegen.make(self.r,self.o)
    def compile(self):
        cc=shutil.which('cc')
        if not cc:self.skipTest('host C compiler unavailable')
        p=subprocess.run([cc,'-std=c99','-Wall','-Wextra','-Werror',
                '-I'+str(ROOT/'tools/review_txn'),str(self.o/'r95_txn_fixture.c'),
                str(ROOT/'tools/review_txn/txn.c'),str(ROOT/'tools/review_txn/sha256.c'),
                '-o',str(self.r/'txn')],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        return self.r/'txn'
    def runexe(self,exe,mode):
        return subprocess.run([str(exe),mode],capture_output=True,text=True)
    def test_real_c_transaction_trial_then_recover(self):
        report=self.build();self.assertEqual(report['scope'],'HOST_FIXTURE_ONLY')
        self.assertFalse(report['installable'])
        exe=self.compile()
        self.assertEqual(self.runexe(exe,'trial').returncode,0)
        self.assertIn('PENDING_HEALTH',self.runexe(exe,'status').stdout)
        for name in ('bluetooth.fixture','si.fixture'):
            self.assertEqual((self.r/'target'/name).read_bytes(),('new-'+name).encode())
            self.assertEqual((self.r/'backup'/name).read_bytes(),('old-'+name).encode())
        self.assertEqual(self.runexe(exe,'recover').returncode,0)
        self.assertIn('RESTORED',self.runexe(exe,'status').stdout)
        for name in ('bluetooth.fixture','si.fixture'):
            self.assertEqual((self.r/'target'/name).read_bytes(),('old-'+name).encode())
        self.assertNotEqual(self.runexe(exe,'confirm').returncode,0)
    def test_bad_target_hash_prevents_first_write(self):
        self.build();exe=self.compile()
        (self.r/'target'/'si.fixture').write_bytes(b'TAMPER')
        self.assertNotEqual(self.runexe(exe,'trial').returncode,0)
        self.assertEqual(list((self.r/'backup').iterdir()),[])
    def test_payload_tamper_prevents_first_write(self):
        self.build();exe=self.compile()
        (self.r/'payload'/'si.fixture').write_bytes(b'TAMPER')
        self.assertNotEqual(self.runexe(exe,'trial').returncode,0)
        self.assertEqual(list((self.r/'backup').iterdir()),[])
    def test_reject_unmarked(self):
        (self.r/codegen.MARKER).unlink()
        with self.assertRaisesRegex(ValueError,'marker'):self.build()
    def test_reject_untrusted_name(self):
        (self.r/'target'/'si.fixture').rename(self.r/'target'/'..si')
        (self.r/'payload'/'si.fixture').rename(self.r/'payload'/'..si')
        with self.assertRaisesRegex(ValueError,'unsafe'):self.build()
    def test_reject_extra_file(self):
        (self.r/'target'/'extra').write_bytes(b'x')
        with self.assertRaisesRegex(ValueError,'names'):self.build()
    def test_reject_symlink(self):
        f=self.r/'payload'/'si.fixture';f.unlink();f.symlink_to(self.r/'target'/'si.fixture')
        with self.assertRaisesRegex(ValueError,'unsafe'):self.build()
    def test_reject_hardlink(self):
        f=self.r/'payload'/'si.fixture';os.link(f,self.r/'payload'/'si-alias')
        (self.r/'target'/'si-alias').write_bytes(b'x')
        with self.assertRaisesRegex(ValueError,'hardlink'):self.build()
    def test_reject_nonempty_backup(self):
        (self.r/'backup'/'existing').write_bytes(b'z')
        with self.assertRaisesRegex(ValueError,'empty'):self.build()
    def test_reject_existing_output(self):
        self.build()
        with self.assertRaisesRegex(ValueError,'output'):self.build()
    def test_reject_wrong_root(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError,'dedicated'):codegen.make(d,self.o)

if __name__=='__main__':unittest.main(verbosity=2)
