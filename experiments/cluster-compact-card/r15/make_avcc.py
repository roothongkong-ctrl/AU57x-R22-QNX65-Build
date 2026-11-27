#!/usr/bin/env python3
"""Turn one actual FFmpeg-generated Annex-B IDR into avcC + AVCC AU."""
import re,sys,struct,pathlib
data=pathlib.Path(sys.argv[1]).read_bytes()
nals=[n for n in re.split(b'\x00\x00\x00\x01|\x00\x00\x01',data) if n]
sps=next(n for n in nals if n[0]&31==7)
pps=next(n for n in nals if n[0]&31==8)
assert any(n[0]&31==5 for n in nals)
config=b'\x01'+sps[1:4]+b'\xff\xe1'+struct.pack('>H',len(sps))+sps+b'\x01'+struct.pack('>H',len(pps))+pps
au=b''.join(struct.pack('>I',len(n))+n for n in nals if n[0]&31 not in (7,8))
pathlib.Path(sys.argv[2]).write_bytes(config)
pathlib.Path(sys.argv[3]).write_bytes(au)
