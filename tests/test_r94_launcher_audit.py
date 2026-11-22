import hashlib
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import r94_launcher_audit as a


def fake():
    d=bytearray(52)
    d[:6] = b'\x7fELF\x01\x01'
    d[16:18] = (2).to_bytes(2,'little')
    d[18:20] = (40).to_bytes(2,'little')
    return bytes(d) + b'\0'.join(a.NATIVE_IMPORTS)


class AuditTests(unittest.TestCase):
    def test_valid_shape_and_independent_hash(self):
        b = fake()
        r = a.audit(b, hashlib.sha256(b).hexdigest())
        self.assertFalse(r['proves_real_btstack_loader'])
        self.assertFalse(r['proves_supervisor_inherits_LD_PRELOAD'])
        self.assertTrue(r['has_connectivity_json_reference'])

    def test_hash_modified(self):
        b = fake()
        with self.assertRaisesRegex(ValueError, 'not pinned'):
            a.audit(b, '0'*64)

    def test_wrong_arm_or_elf_kind(self):
        for pos, val in ((0, 0), (4, 2), (5, 2), (16, 3), (18, 62)):
            b = bytearray(fake()); b[pos] = val
            with self.assertRaises(ValueError):
                a.audit(bytes(b), hashlib.sha256(b).hexdigest())

    def test_missing_required_import(self):
        b=fake().replace(b'connectivity.json', b'connectivity.yml')
        with self.assertRaisesRegex(ValueError, 'missing'):
            a.audit(b, hashlib.sha256(b).hexdigest())


if __name__ == '__main__':unittest.main()
