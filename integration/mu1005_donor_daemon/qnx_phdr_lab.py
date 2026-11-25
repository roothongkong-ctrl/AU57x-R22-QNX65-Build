"""Test the proposed appended PHDR layout on a NON-OEM QNX lab executable."""
import struct,sys
from pathlib import Path
from importlib.util import spec_from_file_location,module_from_spec
spec=spec_from_file_location("obs",Path(__file__).resolve().parents[2]/"tools/mu1005_lifecycle_observer.py")
m=module_from_spec(spec);spec.loader.exec_module(m)
src,dst=map(Path,sys.argv[1:3])
b=src.read_bytes();old=m.headers(b)
assert old[0][0]==6 and sum(x[0]==6 for x in old)==1
assert all(x[2]+x[5]<0x70000000 for x in old if x[0]==1)
base=m.align(max(x[2]+x[5] for x in old if x[0]==1)+0x10000,4096)
state=base+0x10000
n=len(old)+2;sz=n*32;off=m.align(len(b),4096);stateoff=m.align(off+sz,4096)
new=[x[:] for x in old];new[0]=[6,off,base,base,sz,sz,4,4]
new.extend([[1,off,base,base,sz,sz,5,4096],[1,stateoff,state,state,0,4096,6,4096]])
out=bytearray(b);struct.pack_into('<I',out,28,off);struct.pack_into('<H',out,44,n)
out.extend(bytes(off-len(out)));out.extend(b''.join(struct.pack('<8I',*x) for x in new));out.extend(bytes(stateoff-len(out)))
assert m.headers(out)==new and src.read_bytes()==b
dst.write_bytes(out);dst.chmod(0o755)
print("QNX_LAB_APPENDED_PHDR_LAYOUT_CREATED",hex(base),hex(state))
