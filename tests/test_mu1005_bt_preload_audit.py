import unittest
import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('audit',Path(__file__).resolve().parents[1]/'tools/mu1005_bt_preload_audit.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class OriginalPreloadAuditTest(unittest.TestCase):
 def test_arm_header(self):
  b=bytearray(52);b[:6]=b'\x7fELF\x01\x01';b[16:20]=b'\x02\x00\x28\x00';self.assertTrue(m.header_ok(bytes(b)))
  b[18]=0x3e;self.assertFalse(m.header_ok(bytes(b)))
 def test_only_full_exact_callsites(self):
  txt='\n'.join(f'{x:08x}: bl 0x{m.PLT:x} <.plt+0x71c>' for x in m.CALLS)
  self.assertEqual(m.inspect_asm(txt),list(m.CALLS))
  txt=txt.replace('236c48:','236c44:')
  self.assertNotEqual(m.inspect_asm(txt),list(m.CALLS))
 def test_other_plt_rejected(self):
  self.assertEqual(m.inspect_asm(' 236668: bl 0x117394 <some_plt>'),[])
 def test_standalone_bl_pattern(self):
  self.assertEqual(m.inspect_asm(' 236668: b 0x117390 <branch>'),[])
if __name__=='__main__':unittest.main()
