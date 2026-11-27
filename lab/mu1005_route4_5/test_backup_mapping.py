import hashlib,tempfile,unittest
from pathlib import Path
from backup_mapping import check,NAMES
class Test(unittest.TestCase):
 def setUp(self):
    self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.ref=[];self.mapping={}
    for n in NAMES:
      p="copy_"+n;self.mapping[n]=p;(self.root/p).write_bytes(n.encode())
      self.ref.append({"name":n,"sha256":hashlib.sha256(n.encode()).hexdigest(),"bytes":len(n)})
 def tearDown(self):self.tmp.cleanup()
 def test_explicit(self):
    r=check(self.root,self.ref,self.mapping);self.assertEqual(len(r["files"]),10);self.assertFalse(r["installable"]);self.assertFalse(r["full_vehicle_backup_verified"])
 def test_missing(self):
    self.mapping.pop(next(iter(NAMES)))
    with self.assertRaises(ValueError):check(self.root,self.ref,self.mapping)
 def test_swapped(self):
    a,b=list(NAMES)[:2];self.mapping[a],self.mapping[b]=self.mapping[b],self.mapping[a]
    with self.assertRaises(ValueError):check(self.root,self.ref,self.mapping)
 def test_duplicate(self):
    a,b=list(NAMES)[:2];self.mapping[a]=self.mapping[b]
    with self.assertRaises(ValueError):check(self.root,self.ref,self.mapping)
 def test_corrupt(self):
    (self.root/self.mapping[next(iter(NAMES))]).write_bytes(b"bad")
    with self.assertRaises(ValueError):check(self.root,self.ref,self.mapping)
if __name__=="__main__":unittest.main()
