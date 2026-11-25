"""Host synthetic fixtures plus optional private actual EFS regression."""
import hashlib
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("stage",ROOT/"tools/mu1005_efs_launcher_stage.py")
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class Test(unittest.TestCase):
    def test_original_required(self):
        with self.assertRaises(ValueError):m.extract(bytes(2097152))
        with self.assertRaises(ValueError):m.stage_config(b'{}')
        with self.assertRaises(ValueError):m.parse(b'{"a":1,"a":2}')
    def test_comment_preserving_single_field(self):
        c=b'{\n# comment\n"launcher":{"btstack":{"exec":"/eso/bin/apps/btstack","preCondition":"/tmp/mvloaded","maxUnhealthyReports":3}},"btstack":{"stayMaster":true},"bluetooth":{"enableIap":false}}\n'
        with patch.object(m,"CONFIG_SHA",hashlib.sha256(c).hexdigest()):
            result=m.stage_config(c)
        self.assertEqual(result.replace(m.WRAPPER.encode(),m.STOCK.encode(),1),c)
        self.assertEqual(m.parse(result)["btstack"],{"stayMaster":True})
        self.assertFalse(m.parse(result)["bluetooth"]["enableIap"])
        subprocess.run(["sh","-n"],input=m.WRAPPER_TEXT.encode(),check=True)
    @unittest.skipUnless(os.environ.get("MU1005_PRIVATE_EFS"),"private OEM image not supplied")
    def test_real_efs(self):
        image=Path(os.environ["MU1005_PRIVATE_EFS"])
        old=image.read_bytes()
        c=m.extract(old)
        with tempfile.TemporaryDirectory() as t:
            out=Path(t)/"review"
            report=m.run(image,out)
            self.assertFalse(report["installable"])
            self.assertEqual((out/"connectivity.original.json").read_bytes(),c)
            self.assertEqual((out/"connectivity.STAGED_REVIEW_ONLY.json").read_bytes().replace(m.WRAPPER.encode(),m.STOCK.encode(),1),c)
            with self.assertRaises(ValueError):m.run(image,out)
        self.assertEqual(image.read_bytes(),old)

if __name__=="__main__":unittest.main()
