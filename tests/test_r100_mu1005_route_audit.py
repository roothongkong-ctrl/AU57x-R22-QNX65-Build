import hashlib
import pathlib
import struct
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from r100_mu1005_route_audit import check_bytes, audit, PINNED


def fake_elf(et=2,extra=b'RFCOMM'):
    b=bytearray(52)
    b[:7]=b'\x7fELF\x01\x01\x01'
    struct.pack_into('<HH',b,16,et,40)
    return bytes(b)+extra

class Audit(unittest.TestCase):
    def test_strong_pinned_positive(self):
        d=fake_elf(extra=b'REALLY RFCOMM /dev/iapDevice')
        x=check_bytes('btstack',d,pin={'btstack':hashlib.sha256(d).hexdigest()},
                      checks={'btstack':{'a':b'RFCOMM','b':b'IapServices','c':b'/dev/iapDevice'}},
                      types={'btstack':2})
        self.assertTrue(x['markers']['a']['found'])
        self.assertFalse(x['markers']['b']['found'])
        self.assertTrue(x['markers']['c']['found'])
    def test_reject_modified_same_size(self):
        d=fake_elf();alter=bytearray(d);alter[-1]^=1
        with self.assertRaisesRegex(ValueError,'SHA256'):
            check_bytes('btstack',bytes(alter),pin={'btstack':hashlib.sha256(d).hexdigest()},
                        checks={'btstack':{}},types={'btstack':2})
    def test_reject_misidentified_elf(self):
        d=fake_elf();invalid=bytearray(d);invalid[:4]=b'BAD!'
        with self.assertRaisesRegex(ValueError,'ELF'):
            check_bytes('btstack',bytes(invalid),pin={'btstack':hashlib.sha256(invalid).hexdigest()},
                        checks={'btstack':{}},types={'btstack':2})
    def test_reject_non_arm(self):
        d=bytearray(fake_elf());struct.pack_into('<H',d,18,62)
        with self.assertRaisesRegex(ValueError,'machine'):
            check_bytes('btstack',bytes(d),pin={'btstack':hashlib.sha256(d).hexdigest()},
                        checks={'btstack':{}},types={'btstack':2})
    def test_reject_wrong_type(self):
        d=fake_elf(et=3)
        with self.assertRaisesRegex(ValueError,'type'):
            check_bytes('btstack',d,pin={'btstack':hashlib.sha256(d).hexdigest()},
                        checks={'btstack':{}},types={'btstack':2})
    def test_all_real_binary_paths_required(self):
        with self.assertRaisesRegex(ValueError,'five'):
            audit({'btstack':'/dummy'})
    def test_pinned_known_keys(self):
        self.assertEqual(set(PINNED),set(['btstack','bluetooth','ipod-drvr-iap2.so','smartphone_integrator','connectivity_launcher']))

if __name__=='__main__':unittest.main()
