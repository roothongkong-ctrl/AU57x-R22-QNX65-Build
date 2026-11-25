import pathlib,struct,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
from mu1005_bt_trace_entry import direct_address,branch_target,scan

class TraceEntryTests(unittest.TestCase):
    def test_arm_reference(self):
        self.assertEqual(direct_address(0x1000,0xe59f0008,lambda a:{0x1010:0x2000}.get(a)),0x2000)
        self.assertEqual(direct_address(0x1000,0xe51f0004,lambda a:{0x1004:0x3000}.get(a)),0x3000)
        self.assertEqual(direct_address(0x1000,0xe28f0010,lambda _:None),0x1018)
        self.assertEqual(direct_address(0x1000,0xe24f0010,lambda _:None),0xff8)
        self.assertIsNone(direct_address(0x1000,0xe5bf0008,lambda _:1))
    def test_branch(self):
        self.assertEqual(branch_target(0x1000,0xeb000002),0x1010)
        self.assertEqual(branch_target(0x1000,0xeafffffc),0xff8)
        self.assertIsNone(branch_target(0x1000,0xfa000002))
    def test_fixture(self):
        raw=bytearray(128)
        struct.pack_into("<I",raw,0,0xe59f0000)
        struct.pack_into("<I",raw,8,0x2044)
        raw[64:64+20]=b"DBG Device ID ready\0"
        r=scan(raw,[(0x2000,0x2040,0,1),(0x2040,0x2080,64,4)])
        self.assertTrue(any(x["site"]=="0x2000" and x["string_va"]=="0x2044" and "device_id" in x["tags"] for x in r["direct_literal_or_adr_candidates"]))
        self.assertFalse(r["direct_rf_register_branches"])
if __name__=="__main__":unittest.main()
