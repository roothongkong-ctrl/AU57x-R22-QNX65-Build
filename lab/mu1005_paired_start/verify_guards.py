#!/usr/bin/env python3
"""Compare adapter guard words against exact original ELF, never execute it."""
import hashlib,struct,re,sys
from pathlib import Path
d=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(d).hexdigest()=="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
p=struct.unpack_from("<I",d,28)[0];s,n=struct.unpack_from("<HH",d,42)
loads=[(v,v+z,o) for k,o,v,_,z,_,_,_ in (struct.unpack_from("<8I",d,p+i*s) for i in range(n)) if k==1]
errors=[]
for name in sys.argv[2:]:
    pairs=re.findall(r"\{(0x[0-9a-f]+)u,(0x[0-9a-f]+)u\}",Path(name).read_text())
    assert pairs
    for a,w in pairs:
        addr,val=int(a,16),int(w,16)
        actual=next(struct.unpack_from("<I",d,o+addr-lo)[0] for lo,hi,o in loads if lo<=addr and addr+4<=hi)
        if actual!=val:errors.append((name,a,w,hex(actual)))
    print("ORIGINAL_WORDS",name,len(pairs))
print("MISMATCHES",errors)
assert not errors
