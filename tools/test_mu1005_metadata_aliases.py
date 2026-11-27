import json, tempfile, unittest
from pathlib import Path
from mu1005_oem_inventory import metadata_aliases

class TestMetadataAliases(unittest.TestCase):
    def test_resolve_relative_only(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/"tree";root.mkdir()
            (root/"lib").mkdir();(root/"lib"/"libx.so.4.0").write_bytes(b"fixture")
            m=Path(td)/"metadata.json"
            entries=[{"type":"symlink","path":a,"target":b} for a,b in [
                ("lib/libx.so.4","libx.so.4.0"),
                ("lib/libx.so","libx.so.4"),
                ("escape","../outside"),
                ("absolute","/lib/libx.so.4.0"),
                ("cycle1","cycle2"),("cycle2","cycle1"),
                ("duplicate","lib/libx.so.4"),("duplicate","lib/libx.so.4.0"),
                ("missing","lib/absent")]]
            m.write_text(json.dumps({"entries":entries}))
            aliases=metadata_aliases(root,m)
            self.assertEqual(set(aliases),{root/"lib/libx.so.4",root/"lib/libx.so"})
            self.assertEqual(set(aliases.values()),{root/"lib/libx.so.4.0"})
            self.assertFalse((root/"lib/libx.so").exists())
if __name__=="__main__":unittest.main()
