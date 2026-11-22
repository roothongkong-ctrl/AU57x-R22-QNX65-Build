import hashlib
import importlib.util
import struct
import unittest
from pathlib import Path

SRC=Path(__file__).resolve().parents[1]/'tools'/'r93_btstack_plt_proof.py'
spec=importlib.util.spec_from_file_location('r93proof',SRC)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class ProofTest(unittest.TestCase):
    def setUp(self):
        self.blob=bytearray(0x140000)
        self.blob[:6]=b'\x7fELF\x01\x01'
        struct.pack_into('<HH',self.blob,16,2,40)
        self.blob[0xd4:0xd4+len(b'/usr/lib/ldqnx.so.2\0')]=b'/usr/lib/ldqnx.so.2\0'
        for pc in m.CALLERS:
            rel=(m.PLT-(pc+8))//4
            struct.pack_into('<I',self.blob,pc-0x100000,0xeb000000|(rel&0xffffff))
        self.reloc='002c148c  00024e16 R_ARM_JUMP_SLOT   00117390   pthread_create\n'
        self.needed='Shared library: [libc.so.3]'
        self.orig=m.EXPECTED
        m.EXPECTED=hashlib.sha256(self.blob).hexdigest()

    def tearDown(self):m.EXPECTED=self.orig
    def check(self):return m.verify(bytes(self.blob),self.reloc,self.needed)
    def test_clean(self):
        r=self.check();self.assertTrue(r['dynamic_interposition_structurally_possible']);self.assertFalse(r['in_car_loaded'])
    def test_wrong_relocation(self):
        self.reloc=self.reloc.replace('002c148c','002c1480');self.assertRaises(ValueError,self.check)
    def test_wrong_interpreter(self):
        self.blob[0xd4]=ord('X');m.EXPECTED=hashlib.sha256(self.blob).hexdigest();self.assertRaises(ValueError,self.check)
    def test_wrong_branch(self):
        pc=m.CALLERS[0];self.blob[pc-0x100000]=0
        m.EXPECTED=hashlib.sha256(self.blob).hexdigest();self.assertRaises(ValueError,self.check)
    def test_wrong_hash(self):
        self.blob[-1]=1;self.assertRaises(ValueError,self.check)
    def test_missing_libc(self):
        self.needed='';self.assertRaises(ValueError,self.check)
    def test_wrong_cpu(self):
        struct.pack_into('<H',self.blob,18,62);m.EXPECTED=hashlib.sha256(self.blob).hexdigest();self.assertRaises(ValueError,self.check)
if __name__=='__main__':unittest.main()
