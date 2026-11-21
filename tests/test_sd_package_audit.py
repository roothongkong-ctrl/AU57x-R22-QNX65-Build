#!/usr/bin/env python3
"""ZIP safety and manifest tamper tests for non-installable cloud review."""
import hashlib
import io
import json
import stat
import sys
import unittest
import zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from sd_package_audit import inspect_zip,FIRMWARE
def fixture(payload=b"test-only",filename="payload/note.txt",changes=None,extra=None):
    manifest={"firmware":FIRMWARE,"review_only":True,"installable":False,
              "files":[{"path":filename,"sha256":hashlib.sha256(payload).hexdigest()}]}
    if changes:changes(manifest)
    b=io.BytesIO()
    with zipfile.ZipFile(b,"w") as z:
        z.writestr("manifest.json",json.dumps(manifest))
        z.writestr(filename,payload)
        if extra:
            for name,data in extra:z.writestr(name,data)
    b.seek(0)
    return b
class SDReviewTest(unittest.TestCase):
    def test_valid_dry_run_package(self):
        self.assertEqual(inspect_zip(fixture()),[])
    def test_unknown_firmware(self):
        self.assertTrue(inspect_zip(fixture(changes=lambda m:m.update(firmware="MU0000"))))
    def test_flash_claim(self):
        self.assertTrue(inspect_zip(fixture(changes=lambda m:m.update(installable=True))))
    def test_bad_hash(self):
        self.assertTrue(inspect_zip(fixture(changes=lambda m:m["files"][0].update(sha256="0"*64))))
    def test_traversal(self):
        self.assertTrue(inspect_zip(fixture(filename="../escape.txt")))
    def test_executable_not_allowed(self):
        self.assertTrue(inspect_zip(fixture(filename="payload/install.sh")))
    def test_undeclared_member(self):
        self.assertTrue(inspect_zip(fixture(extra=[("payload/extra.txt",b"bad")])))
    def test_duplicate_member(self):
        self.assertTrue(inspect_zip(fixture(extra=[("payload/note.txt",b"bad")])))
    def test_large_member(self):
        self.assertTrue(inspect_zip(fixture(payload=b"X"*(512*1024+1))))
    def test_broken_manifest(self):
        b=io.BytesIO()
        with zipfile.ZipFile(b,"w") as z:z.writestr("manifest.json","NOT_JSON")
        b.seek(0)
        self.assertTrue(inspect_zip(b))
if __name__=="__main__":unittest.main(verbosity=2)
