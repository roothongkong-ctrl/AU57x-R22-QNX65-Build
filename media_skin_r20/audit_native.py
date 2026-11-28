#!/usr/bin/env python3
"""Read-only exact-build native ordering evidence; not a QNX runtime test."""
import hashlib, struct, sys
from pathlib import Path
from bisect import bisect_left
raw=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(raw).hexdigest()=='d69de969dbd3082219e9a6520288c91ca652608921b1abd73566163bb3bc4c6f'
assert raw[:6]==b'\x7fELF\x01\x01' and struct.unpack_from('<H',raw,18)[0]==40
phoff=struct.unpack_from('<I',raw,28)[0]; entsize,n=struct.unpack_from('<HH',raw,42)
segments=[struct.unpack_from('<8I',raw,phoff+i*entsize) for i in range(n)]
def word(a):
    for t,o,v,pa,fs,ms,flags,align in segments:
        if t==1 and v<=a and a+4<=v+fs:return struct.unpack_from('<I',raw,o+a-v)[0]
    raise ValueError(hex(a))
checks={
    'JNI forwards depth and calls native insertion': {0x14bbc:0xe59d2010,0x14bc0:0xeb0016cf},
    'insertion scans ascending signed depth': {0x1a7e0:0xe59d0000,0x1a7e4:0xe5932004,0x1a7e8:0xe1500002,0x1a7f4:0xda000089,0x1a804:0xe5911004,0x1a808:0xe1500001,0x1a80c:0xda000083,0x1a834:0xe5831004},
    'native drawable traversal from highest index down': {0x1a3bc:0xe5904094,0x1a3c8:0xe2544001,0x1a3d0:0xe1a05184,0x1a3d4:0xe5963090,0x1a3dc:0xe7930005,0x1a3e0:0xe2455008,0x1a3e4:0xeb0032cd,0x1a3e8:0xe2544001,0x1a3ec:0x5afffff8},
}
for name,items in checks.items():
    for a,w in items.items():assert word(a)==w,(name,hex(a))
    print('PASS:',name)
entries=[]
for name,depth in [('frame',250),('icon',260),('overlay',51),('custom background',500),('stock root background',1000)]:
    entries.insert(bisect_left([x[0] for x in entries],depth),(depth,name))
assert [x[0] for x in reversed(entries)]==[1000,500,260,250,51]
print('PASS: reconstructed same-composite relative ordering; not a frame render')
