"""Synthetic-only negative checks: no donor binaries required in CI."""
import io
import os
import sys
import tarfile
import tempfile
import unittest
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from wcp_donor_reference_audit import WATCH, audit


def sample_tar(add_traversal=False):
    blob = io.BytesIO()
    with tarfile.open(fileobj=blob, mode='w:gz') as t:
        for name in WATCH:
            data = (b'\x7fELF\x01\x01' + b'\0' * 60) if name.endswith('.so') else b'dummy'
            info = tarfile.TarInfo('./mnt/ota/wcp/' + name)
            info.size = len(data)
            t.addfile(info, io.BytesIO(data))
        if add_traversal:
            data = b'bad'
            info = tarfile.TarInfo('../../escape')
            info.size = len(data)
            t.addfile(info, io.BytesIO(data))
    return blob.getvalue()


def zip_ref(tar_data, suffix='MHI2QC_3638/wcp.tar.gz'):
    with tempfile.NamedTemporaryFile(suffix='.zip', delete=False) as f:
        path = f.name
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr('MHI2Q_WiFi_CarPlay-main/' + suffix, tar_data)
    return path


class DonorAuditTest(unittest.TestCase):
    def test_synthetic_manifest(self):
        p = zip_ref(sample_tar())
        try:
            d = audit(p)
            self.assertEqual(set(WATCH), set(d['components']))
            self.assertFalse(d['safe_to_flash'])
            self.assertFalse(d['vendor_code_embedded'])
        finally:
            os.unlink(p)

    def test_reject_path_traversal(self):
        p = zip_ref(sample_tar(True))
        try:
            with self.assertRaisesRegex(ValueError, 'unsafe'):
                audit(p)
        finally:
            os.unlink(p)

    def test_reject_wrong_firmware(self):
        p = zip_ref(sample_tar(), 'MHI2Q_AU57x_fake/wcp.tar.gz')
        try:
            with self.assertRaises(ValueError):
                audit(p)
        finally:
            os.unlink(p)

if __name__ == '__main__':
    unittest.main()
