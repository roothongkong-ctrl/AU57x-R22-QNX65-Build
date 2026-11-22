"""Host-only checks for R86 pinned offline OEM delegate map.

No proprietary firmware is in this repository. The exact-OEM positive gate
can be run locally with MU1005_DIO, MU1005_AIRPLAY and MU1005_SI paths.
"""
import importlib.util
import os
from pathlib import Path
import struct
import sys
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'mu1005_oem_delegate_audit.py'
_spec = importlib.util.spec_from_file_location('mu_oem_audit', SCRIPT)
mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mod)


class FakeArm:
    def __init__(self, words, start=0x1000):
        self.words = words
        self.sections = {'.text': {'addr': start, 'size': 4 * len(words)}}
    def word(self, addr):
        return self.words[(addr-0x1000)//4]


class EvidenceTests(unittest.TestCase):
    def test_elf32_arm_rejection(self):
        for blob in (b'', b'\x7fELF\x02\x01' + bytes(100),
                     b'\x7fELF\x01\x02' + bytes(100)):
            with self.assertRaises(mod.AuditError):
                mod.ELF(blob)

    def test_unconditional_arm_bl_resolves_forward_backward(self):
        # Branch source 0x1000 => 0x100c, source 0x1008 => 0x1000.
        fake = FakeArm([0xeb000001, 0xe1a00000, 0xebfffffc, 0xe12fff1e])
        self.assertEqual(mod.ELF.branch_calls(fake, 0x100c), [0x1000])
        self.assertEqual(mod.ELF.branch_calls(fake, 0x1000), [0x1008])
        self.assertEqual(mod.ELF.branch_calls(fake, 0x1010), [])

    def test_wrong_insn_or_fake_oem_never_passes(self):
        fake = FakeArm([0xe1a00000])
        with self.assertRaises(mod.AuditError):
            mod.must(fake, 0x1000, 0xe3a00000)
        with self.assertRaises(mod.AuditError):
            mod.audit({'dio_manager': '/dev/null',
                       'libairplay.so': '/dev/null',
                       'smartphone_integrator': '/dev/null'})

    @unittest.skipUnless(all(os.environ.get(k) for k in
             ('MU1005_DIO', 'MU1005_AIRPLAY', 'MU1005_SI')),
             'original proprietary binaries not in public CI')
    def test_real_binaries_and_tamper_fail_closed(self):
        filemap = {'dio_manager': os.environ['MU1005_DIO'],
                   'libairplay.so': os.environ['MU1005_AIRPLAY'],
                   'smartphone_integrator': os.environ['MU1005_SI']}
        r = mod.audit(filemap)
        self.assertFalse(r['safe_to_flash'])
        self.assertFalse(r['vehicle_verified'])
        self.assertEqual(r['DIO_delegate']['delegate_size'], 60)
        self.assertEqual(len(r['DIO_delegate']['callbacks']), 11)
        self.assertEqual(len(r['AirPlay_session_delegate']['callbacks']), 10)
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as d:
            src = Path(filemap['dio_manager']).read_bytes()
            changed = bytearray(src)
            changed[0x15df68-0x100000] ^= 1
            bad = Path(d) / 'corrupt_dio'
            bad.write_bytes(changed)
            with self.assertRaisesRegex(mod.AuditError, 'exact pinned'):
                mod.audit({**filemap, 'dio_manager': str(bad)})


if __name__ == '__main__':
    unittest.main()
