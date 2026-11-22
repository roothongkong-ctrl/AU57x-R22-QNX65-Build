#!/usr/bin/env python3
"""Exact private MU1005 btstack dynamic loader/PLT evidence; zero OEM writes."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

EXPECTED = '51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566'
CALLERS = (0x236668, 0x2368c8, 0x236c2c, 0x236c48)
PLT = 0x117390
GOT = 0x2c148c

def verify(data: bytes, relocations: str, needed: str) -> dict:
    if hashlib.sha256(data).hexdigest() != EXPECTED:
        raise ValueError('original MU1005 btstack SHA mismatch')
    if (data[:4] != b'\x7fELF' or data[4:6] != b'\x01\x01'
            or struct.unpack_from('<HH', data, 16) != (2, 40)):
        raise ValueError('not ET_EXEC ARM ELF32 LE')
    if b'/usr/lib/ldqnx.so.2\0' not in data[:0x120]:
        raise ValueError('original QNX dynamic interpreter mismatch')
    if 'Shared library: [libc.so.3]' not in needed:
        raise ValueError('QNX libc is not an original dependency')
    match = re.search(r'^\s*002c148c\s+[0-9a-fA-F]+\s+R_ARM_JUMP_SLOT\s+[0-9a-fA-F]+\s+pthread_create\s*$', relocations, re.MULTILINE)
    if match is None:
        raise ValueError('exact original pthread_create relocation GOT mismatch')
    calls=[]
    for pc in CALLERS:
        off=pc-0x100000
        insn=struct.unpack_from('<I', data, off)[0]
        if (insn >> 24) != 0xeb:
            raise ValueError(f'caller at {pc:#x} is not ARM BL')
        imm = insn & 0xffffff
        if imm & 0x800000: imm-=1<<24
        dst=(pc+8+(imm<<2)) & 0xffffffff
        if dst != PLT:
            raise ValueError(f'original caller {pc:#x} does not branch to target PLT')
        calls.append({'arm_bl_site':hex(pc),'target':hex(dst),'instruction':hex(insn)})
    return {'target':'MHI2_CN_AU57x_K1006_2/MU1005',
            'original_btstack_sha256':EXPECTED,'loader':'/usr/lib/ldqnx.so.2',
            'original_got_pthread_create':hex(GOT),'original_plt_pthread_create':hex(PLT),
            'verified_calls':calls,'dynamic_interposition_structurally_possible':True,
            'in_car_loaded':False,'in_car_btstack_owner_verified':False,
            'meaning':'binary callsites/relocations proven; no car or QNX execution'}

def audit(path:Path)->dict:
    s=path.read_bytes()
    reloc=subprocess.run(['readelf','-rW',str(path)],check=True,capture_output=True,text=True).stdout
    needed=subprocess.run(['readelf','-dW',str(path)],check=True,capture_output=True,text=True).stdout
    return verify(s,reloc,needed)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--btstack',required=True);ap.add_argument('--out');a=ap.parse_args()
    proof=audit(Path(a.btstack))
    out=json.dumps(proof,ensure_ascii=False,indent=2)+'\n'
    if a.out:Path(a.out).write_text(out,encoding='utf-8')
    print(out)
if __name__=='__main__': main()
