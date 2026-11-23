import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import r96_candidate_bundle as gen
from sd_package_audit import inspect_zip


class CandidateBundle(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory(prefix='r96-candidate-')
        self.root=Path(self.t.name)
        self.src=self.root/'evidence.json'
        self.src.write_bytes((ROOT/'r95_result/R95_STEPS_03_TO_17_EVIDENCE.json').read_bytes())
        self.docs=ROOT/'docs'
        self.out=self.root/'candidate.zip'

    def tearDown(self):self.t.cleanup()

    def build(self):return gen.build(self.src,self.out,self.docs)

    def test_review_zip_integrity_and_provenance(self):
        report=self.build()
        self.assertEqual(len(report['unresolved_production_proofs']),10)
        self.assertFalse(report['installable'])
        self.assertEqual(inspect_zip(self.out),[])
        with zipfile.ZipFile(self.out) as z:
            names=z.namelist()
            self.assertEqual(len(names),5)
            self.assertFalse(any(n.endswith(('.sh','.so','.bin','.c')) for n in names))
            status=json.loads(z.read('payload/R96_STATUS.json'))
            self.assertFalse(status['auto_install_in_car'])
            self.assertEqual(status['steps_03_to_17_vehicle_complete_count'],0)

    def test_deterministic_bytes(self):
        self.build();x=self.out.read_bytes();self.out.unlink();self.build()
        self.assertEqual(x,self.out.read_bytes())

    def test_reject_zip_overwrite(self):
        self.build()
        with self.assertRaisesRegex(ValueError,'overwrite'):self.build()

    def test_reject_forged_vehicle_proof(self):
        j=json.loads(self.src.read_text());j['vehicle_verified']=True
        self.src.write_text(json.dumps(j))
        with self.assertRaisesRegex(ValueError,'evidence'):self.build()

    def test_reject_stage_list_truncation(self):
        j=json.loads(self.src.read_text());j['steps']=j['steps'][:-1]
        self.src.write_text(json.dumps(j))
        with self.assertRaisesRegex(ValueError,'evidence'):self.build()

    def test_reject_failed_offline_step(self):
        j=json.loads(self.src.read_text());j['steps'][5]['offline_test']='FAIL'
        self.src.write_text(json.dumps(j))
        with self.assertRaisesRegex(ValueError,'evidence'):self.build()

    def test_reject_symlink_evidence(self):
        s=self.src.read_bytes();self.src.unlink();original=self.root/'other';original.write_bytes(s)
        self.src.symlink_to(original)
        with self.assertRaisesRegex(ValueError,'unsafe'):self.build()

    def test_forged_installable_manifest_rejected(self):
        self.build();bad=self.root/'bad.zip'
        with zipfile.ZipFile(self.out) as z,zipfile.ZipFile(bad,'w') as out:
            for n in z.namelist():
                data=z.read(n)
                if n=='manifest.json':
                    d=json.loads(data);d['installable']=True;data=json.dumps(d).encode()
                out.writestr(n,data)
        self.assertNotEqual(inspect_zip(bad),[])

    def test_forged_member_rejected(self):
        self.build();bad=self.root/'bad.zip'
        with zipfile.ZipFile(self.out) as z,zipfile.ZipFile(bad,'w') as out:
            for n in z.namelist():out.writestr(n,z.read(n))
            out.writestr('payload/extra.sh','echo BAD')
        self.assertNotEqual(inspect_zip(bad),[])

if __name__=='__main__':unittest.main(verbosity=2)
