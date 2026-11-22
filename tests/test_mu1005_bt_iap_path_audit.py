import importlib.util
import pathlib
import struct
import tempfile
import unittest

P=pathlib.Path(__file__).resolve().parents[1]/"tools"/"mu1005_bt_iap_path_audit.py"
spec=importlib.util.spec_from_file_location("mu1005_bt_iap_path_audit",P)
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)


def sample_elf():
    b=bytearray(256)
    b[:16]=b"\x7fELF\x01\x01\x01"+b"\0"*9
    struct.pack_into("<HH",b,16,2,40)
    struct.pack_into("<I",b,28,52)
    struct.pack_into("<HH",b,42,32,2)
    struct.pack_into("<8I",b,52,3,164,0,0,20,20,4,1)
    struct.pack_into("<8I",b,84,1,0,0x100000,0,256,256,5,0x1000)
    b[164:184]=b"/usr/lib/ldqnx.so.2\x00"
    return b

class Audit(unittest.TestCase):
    def test_pinned_hash_rejects_unrelated_original(self):
        with tempfile.TemporaryDirectory() as d:
            f=pathlib.Path(d)/"btstack";f.write_bytes(sample_elf())
            with self.assertRaisesRegex(ValueError,"hash mismatch"):
                a.read_verified(f,a.BT_SHA)
    def test_arm_qnx_parser(self):
        b=sample_elf();self.assertEqual(len(a.elf32_arm_qnx(b)),1)
        b[18]=62
        with self.assertRaisesRegex(ValueError,"ARM32"):
            a.elf32_arm_qnx(b)
    def test_invalid_segment_rejected(self):
        b=sample_elf();struct.pack_into("<I",b,84+16,257)
        with self.assertRaisesRegex(ValueError,"segment"):
            a.elf32_arm_qnx(b)
    def test_pinned_instruction_and_tamper(self):
        b=sample_elf();struct.pack_into("<I",b,216,0xe92d4070)
        loads=a.elf32_arm_qnx(b)
        self.assertEqual(a.check_words(b,loads,[(0x1000d8,0xe92d4070)]),["0x001000d8"])
        with self.assertRaisesRegex(ValueError,"differs"):
            a.check_words(b,loads,[(0x1000d8,0xabcdef00)])
        with self.assertRaisesRegex(ValueError,"not in original"):
            a.check_words(b,loads,[(0x900000,0)])
    def test_words_from_original_source_header(self):
        p=P.parent.parent/"project/source/legacy_mu1005/bluetooth/native_binding_words.h"
        self.assertEqual(len(a.words_from_header(p)),40)
    def test_marker_stays_negative_without_evidence(self):
        m=a.scan(sample_elf(),["joey_MU0678_dev_iapDevice"])
        self.assertFalse(m["joey_MU0678_dev_iapDevice"]["present"])

if __name__=="__main__":unittest.main()
