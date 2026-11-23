"""Use real POSIX C file operations, rollback journals and actual child processes."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import r96_oneclick_codegen as gen
import r95_sd_plan_codegen as base


class OneClickLab(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='au57x-r95-')
        self.r = Path(self.temp.name)
        (self.r/base.MARKER).write_text('FIXTURE_ONLY\n')
        for d in ('target','backup','payload','state'):
            (self.r/d).mkdir(mode=0o700)
        self.names = ('btstack.fixture','si.fixture','dio.fixture')
        for name in self.names:
            (self.r/'target'/name).write_bytes(b'ORIGINAL:'+name.encode())
            (self.r/'payload'/name).write_bytes(b'CANDIDATE:'+name.encode())
        self.out = self.r/'gen'
        self.bin = self.r/'r96-lab'

    def tearDown(self):
        self.temp.cleanup()

    def compile(self):
        report=gen.generate(self.r,self.out)
        self.assertFalse(report['installable'])
        cc=shutil.which('gcc') or shutil.which('cc')
        if cc is None:self.skipTest('no C compiler')
        code = ROOT/'tools/review_txn'
        argv=[cc,'-std=c99','-Wall','-Wextra','-Werror','-pedantic',
              '-I'+str(code),str(self.out/'r96_oneclick_HOST_LAB.c'),
              str(code/'txn.c'),str(code/'sha256.c'),str(code/'supervisor_fixture.c'),
              '-o',str(self.bin)]
        r=subprocess.run(argv,capture_output=True,text=True)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        return self.bin

    def runbin(self,action,mode=None):
        env=os.environ.copy()
        if mode:env['TXN_LIFE_MODE']=mode
        return subprocess.run([str(self.bin),action],env=env,capture_output=True,text=True,timeout=10)

    def phase(self):
        return self.runbin('status').stdout

    def check_bytes(self,prefix):
        for n in self.names:
            self.assertEqual((self.r/'target'/n).read_bytes(),prefix+n.encode())

    def test_oneclick_real_supervised_install_restore(self):
        self.compile()
        p=self.runbin('oneclick')
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        self.assertIn('COMMITTED',self.phase())
        self.check_bytes(b'CANDIDATE:')
        for n in self.names:
            self.assertEqual((self.r/'backup'/n).read_bytes(),b'ORIGINAL:'+n.encode())
        p=self.runbin('restore')
        self.assertEqual(p.returncode,3,p.stdout+p.stderr)
        self.assertIn('RESTORED',self.phase())
        self.check_bytes(b'ORIGINAL:')

    def test_health_failure_restores_original(self):
        self.compile();p=self.runbin('oneclick','bad-health')
        self.assertEqual(p.returncode,3,p.stdout+p.stderr)
        self.assertIn('RESTORED',self.phase());self.check_bytes(b'ORIGINAL:')

    def test_timeout_restores_original(self):
        self.compile();p=self.runbin('oneclick','timeout')
        self.assertEqual(p.returncode,3,p.stdout+p.stderr)
        self.assertIn('RESTORED',self.phase());self.check_bytes(b'ORIGINAL:')

    def test_start_failure_restores_original(self):
        self.compile();p=self.runbin('oneclick','start-fails')
        self.assertEqual(p.returncode,3,p.stdout+p.stderr)
        self.assertIn('RESTORED',self.phase());self.check_bytes(b'ORIGINAL:')

    def test_corrupt_original_denies_before_first_copy(self):
        self.compile()
        (self.r/'target'/self.names[0]).write_bytes(b'TAMPER')
        p=self.runbin('oneclick')
        self.assertNotEqual(p.returncode,0)
        self.assertFalse(any((self.r/'backup').iterdir()))

    def test_corrupt_payload_denies_before_first_copy(self):
        self.compile()
        (self.r/'payload'/self.names[0]).write_bytes(b'TAMPER')
        p=self.runbin('oneclick')
        self.assertNotEqual(p.returncode,0)
        self.assertFalse(any((self.r/'backup').iterdir()))

    def test_child_crash_then_recover(self):
        self.compile()
        p=self.runbin('oneclick','crash-after-start')
        self.assertNotEqual(p.returncode,0)
        self.assertIn('PENDING_HEALTH',self.phase())
        q=self.runbin('recover')
        self.assertEqual(q.returncode,3,q.stdout+q.stderr)
        self.check_bytes(b'ORIGINAL:')

    def test_boot_again_with_health_probe(self):
        self.compile();p=self.runbin('oneclick')
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        p=self.runbin('boot','bad-health')
        self.assertEqual(p.returncode,3,p.stdout+p.stderr)
        self.check_bytes(b'ORIGINAL:')

    def test_prepared_plan_cannot_create_twice(self):
        self.compile()
        with self.assertRaisesRegex(ValueError,'output'):
            gen.generate(self.r,self.out)

    def test_missing_marker_refuses_codegen(self):
        (self.r/base.MARKER).unlink()
        with self.assertRaisesRegex(ValueError,'marker'):
            gen.generate(self.r,self.out)

    def test_reject_arbitrary_action(self):
        self.compile()
        p=self.runbin('confirm')
        self.assertEqual(p.returncode,2)
        self.assertFalse(any((self.r/'backup').iterdir()))

    def test_signed_manifest_marks_lifecycle_mock(self):
        self.compile()
        d=json.loads((self.out/'R96_ONECLICK_LAB_MANIFEST.json').read_text())
        self.assertEqual(d['scope'],'HOST_LAB_ONLY')
        for k in ('installable','original_mfi_verified','factory_supervisor_bound',
                  'boot_recovery_vehicle_bound','production_health_callbacks'):
            self.assertFalse(d[k],k)

if __name__=='__main__':
    unittest.main(verbosity=2)
