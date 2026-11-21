import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from cloud_sd_transaction_sim import ReviewFailure,apply_review,digest,MARKER

class TransactionTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="au57x-sim-")
        self.root=Path(self.tmp.name)
        (self.root/MARKER).write_text("SIMULATION_ONLY\n",encoding="ascii")
        (self.root/"fake").mkdir()
        (self.root/"fake/a.txt").write_bytes(b"oldA")
        (self.root/"fake/b.txt").write_bytes(b"oldB")
    def tearDown(self):
        self.tmp.cleanup()
    def plan(self):
        return [
          dict(path="fake/a.txt",old_sha256=digest(b"oldA"),
               new_sha256=digest(b"newA"),payload=b"newA"),
          dict(path="fake/b.txt",old_sha256=digest(b"oldB"),
               new_sha256=digest(b"newB"),payload=b"newB")]
    def contents(self):
        return [(self.root/f"fake/{p}").read_bytes() for p in ("a.txt","b.txt")]
    def test_all_good(self):
        r=apply_review(self.root,self.plan())
        self.assertFalse(r["vehicle_installable"])
        self.assertEqual(self.contents(),[b"newA",b"newB"])
    def test_rollback_first(self):
        with self.assertRaises(ReviewFailure):
            apply_review(self.root,self.plan(),fail_after=1)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_rollback_second(self):
        with self.assertRaises(ReviewFailure):
            apply_review(self.root,self.plan(),fail_after=2)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_bad_old_digest_blocks_all(self):
        p=self.plan();p[1]["old_sha256"]="0"*64
        with self.assertRaises(ReviewFailure):apply_review(self.root,p)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_bad_new_digest_blocks_all(self):
        p=self.plan();p[1]["payload"]=b"tampered"
        with self.assertRaises(ReviewFailure):apply_review(self.root,p)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_duplicate_blocks_all(self):
        p=self.plan();p.append(p[0])
        with self.assertRaises(ReviewFailure):apply_review(self.root,p)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_path_traversal(self):
        p=self.plan();p[1]["path"]="../fake/b.txt"
        with self.assertRaises(ReviewFailure):apply_review(self.root,p)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_symlink_rejected(self):
        (self.root/"fake/a.txt").unlink()
        (self.root/"fake/a.txt").symlink_to(self.root/"fake/b.txt")
        with self.assertRaises(ReviewFailure):apply_review(self.root,self.plan())
        self.assertEqual((self.root/"fake/b.txt").read_bytes(),b"oldB")
    def test_no_marker(self):
        (self.root/MARKER).unlink()
        with self.assertRaises(ReviewFailure):apply_review(self.root,self.plan())
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
    def test_oversized_payload(self):
        p=self.plan();p[0]["payload"]=b"x"*(256*1024+1)
        p[0]["new_sha256"]=digest(p[0]["payload"])
        with self.assertRaises(ReviewFailure):apply_review(self.root,p)
        self.assertEqual(self.contents(),[b"oldA",b"oldB"])
if __name__=="__main__":unittest.main(verbosity=2)
