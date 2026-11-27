import unittest
from prepare_reference import prepare,NAMES
def sample():
 return {"scope":"EXTRACTED_FIRMWARE_PATHS_ONLY","runtime_path_verified":False,"files":[{"name":n,"archive_path":"app70/x/default/app.img/eso/bin/apps/"+n,"sha256":"a"*64,"bytes":8} for n in NAMES]}
class Test(unittest.TestCase):
 def test_complete(self):
  r=prepare(sample());self.assertEqual(len(r),10);self.assertTrue(all(x["path"].startswith("app/") for x in r))
 def test_missing(self):
  x=sample();x["files"].pop()
  with self.assertRaises(ValueError):prepare(x)
 def test_ambiguous(self):
  x=sample();x["files"].append(x["files"][0])
  with self.assertRaises(ValueError):prepare(x)
 def test_escape(self):
  x=sample();x["files"][0]["archive_path"]="app70/x/default/app.img/../"+x["files"][0]["name"]
  with self.assertRaises(ValueError):prepare(x)
 def test_live_claim(self):
  x=sample();x["runtime_path_verified"]=True
  with self.assertRaises(ValueError):prepare(x)
if __name__=="__main__":unittest.main()
