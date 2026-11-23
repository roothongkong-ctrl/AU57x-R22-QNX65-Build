import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from r97_stock_hfp_got_audit import parse_relocations,writable_load_contains

OK = '''Relocation section '.rel.dyn':
002c23c8  0004b615 R_ARM_GLOB_DAT         002481f0   HfRfCallback
002c18c8  00008615 R_ARM_GLOB_DAT         0024c578   SimClientRfCallback
'''
LOAD = '''  LOAD           0x1b97dc 0x002ba7dc 0x002ba7dc 0x007ec0 0x024dfa4 RW  0x1000
  LOAD           0x000000 0x00100000 0x00100000 0x1b97dc 0x1b97dc R E 0x1000
'''

class R97GotAudit(unittest.TestCase):
 def test_exact(self):self.assertEqual(len(parse_relocations(OK)),2)
 def test_wrong_original_va(self):
  with self.assertRaises(ValueError):parse_relocations(OK.replace('002481f0','002481f4'))
 def test_wrong_slot(self):
  with self.assertRaises(ValueError):parse_relocations(OK.replace('002c23c8','002c23cc'))
 def test_missing_sim(self):
  with self.assertRaises(ValueError):parse_relocations(OK.splitlines()[0]+'\n'+OK.splitlines()[1])
 def test_duplicate(self):
  with self.assertRaises(ValueError):parse_relocations(OK+OK.splitlines()[1]+'\n')
 def test_wrong_type(self):
  with self.assertRaises(ValueError):parse_relocations(OK.replace('R_ARM_GLOB_DAT','R_ARM_JUMP_SLOT'))
 def test_writable(self):
  self.assertTrue(writable_load_contains(LOAD,0x2c23c8))
  self.assertTrue(writable_load_contains(LOAD,0x2c18c8))
  self.assertFalse(writable_load_contains(LOAD,0x002481f0))
 def test_non_writable(self):
  self.assertFalse(writable_load_contains(LOAD.replace(' RW ',' R E '),0x2c23c8))

if __name__=='__main__':unittest.main()
