"""Regression on exact OEM seed; stripped copy is synthetic, never executed."""
import sys,struct,tempfile
from pathlib import Path
from mu1005_oem_inventory import inventory,dynamic_inventory,sha,PIN
src=Path(sys.argv[1])
assert sha(src)==PIN["btstack"]
original=inventory(src)
dyn=dynamic_inventory(src,original["elf_type"])
assert set(dyn["needed"])==set(original["needed"]) and len(dyn["needed"])==11
with tempfile.TemporaryDirectory() as td:
    b=bytearray(src.read_bytes())
    struct.pack_into("<I",b,32,0)
    struct.pack_into("<HHH",b,46,0,0,0)
    copy=Path(td)/"sectionless-fixture"
    copy.write_bytes(b)
    got=inventory(copy)
    assert got["needed"]==dyn["needed"]
    assert got["candidate_symbols"]==dyn["candidate_symbols"]
    assert got["candidate_symbols"],"dynamic symbols missing"
    print("OEM_BTSTACK_SECTIONLESS_DYNAMIC_NEEDED_PASS",len(got["needed"]),len(got["candidate_symbols"]))
