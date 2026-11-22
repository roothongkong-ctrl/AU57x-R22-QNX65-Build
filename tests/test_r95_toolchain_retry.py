from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from r95_toolchain_retry import patch,TARGETS

class DockerRetry(unittest.TestCase):
    def test_two_exact_sites(self):
        content='\n'.join(TARGETS)+ '\nsha256sum -c -\nsha256sum -c -\n'
        updated=patch(content)
        self.assertEqual(updated.count('--retry 6'),2)
        self.assertEqual(updated.count('sha256sum -c -'),2)
        self.assertNotIn('curl -fsSL',updated)
    def test_missing_site_rejected(self):
        with self.assertRaises(ValueError):patch(TARGETS[0]+'\nsha256sum -c -\n')
    def test_no_hash_rejected(self):
        with self.assertRaises(ValueError):patch('\n'.join(TARGETS))
    def test_no_infinite_retries(self):
        with self.assertRaises(ValueError):patch(patch('\n'.join(TARGETS)+'\nsha256sum -c -\nsha256sum -c -\n'))
if __name__=='__main__':unittest.main()
