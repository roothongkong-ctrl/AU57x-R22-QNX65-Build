#!/usr/bin/env python3
import importlib.util
import pathlib
import struct
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('audit',ROOT/'tools/r101_rfcomm_scn_audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)

def mock_elf():
    # Fabricated ELF32 with one RX PT_LOAD. No private OEM binary in CI.
    base=0x100000;buf=bytearray(0x180000)
    buf[0:16]=b'\x7fELF\x01\x01\x01'+bytes(9)
    struct.pack_into('<H',buf,16,2);struct.pack_into('<H',buf,18,40)
    struct.pack_into('<I',buf,28,52);struct.pack_into('<H',buf,42,32)
    struct.pack_into('<H',buf,44,1)
    struct.pack_into('<8I',buf,52,1,0,base,base,len(buf),len(buf),5,4096)
    for addr,insn in audit.SITES.items():struct.pack_into('<I',buf,addr-base,insn)
    return buf

class OriginalRFRegisterStructuralTest(unittest.TestCase):
    def test_valid_mock_structure_never_means_vehicle_works(self):
        out=audit.audit(bytes(mock_elf()),check_sha=False)
        self.assertTrue(out['elf32_arm_et_exec_pinned'])
        self.assertFalse(out['iap2_scn_discovered'])
        self.assertFalse(out['installable'])
        self.assertEqual(out['rfcomm_service_channel_range'],[1,30])
    def test_reject_mock_when_pinned_sha_required(self):
        with self.assertRaisesRegex(ValueError,'SHA256'):
            audit.audit(bytes(mock_elf()))
    def test_each_instruction_mutation_refuses(self):
        for addr in audit.SITES:
            with self.subTest(address=hex(addr)):
                buf=mock_elf();buf[addr-0x100000]^=0x01
                with self.assertRaisesRegex(ValueError,'instruction differs'):
                    audit.audit(bytes(buf),check_sha=False)
    def test_bl_targets_same_registration(self):
        self.assertEqual(audit.bl_target(0x2464b0,0xeb00c558),0x277a18)
        self.assertEqual(audit.bl_target(0x24dcac,0xeb00a759),0x277a18)
    def test_no_chance_executing_or_modifying_original(self):
        src=(ROOT/'tools/r101_rfcomm_scn_audit.py').read_text()
        self.assertNotIn('subprocess.run',src)
        self.assertNotIn('os.system',src)
        self.assertNotIn('PATCH_ORIGINAL',src)
    def test_reject_truncated_header(self):
        with self.assertRaises(ValueError):audit.audit(b'NOT_ELF',check_sha=False)
    def test_reject_unsupported_arch(self):
        buf=mock_elf();struct.pack_into('<H',buf,18,62)
        with self.assertRaises(ValueError):audit.audit(bytes(buf),check_sha=False)

if __name__=='__main__':unittest.main(verbosity=2)
