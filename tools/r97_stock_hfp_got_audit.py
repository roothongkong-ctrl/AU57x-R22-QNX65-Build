#!/usr/bin/env python3
"""Read-only original MU1005 btstack HFP/SIM callback GOT relocation proof.
The private OEM executable NEVER ships with the public source tree.
The returned positions are original ET_EXEC virtual addresses, not offsets
portable to different firmware or evidence of actual in-car loading.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

OEM_SHA = '51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566'
RELOCS = {
    'HfRfCallback': (0x2c23c8, 0x2481f0),
    'SimClientRfCallback': (0x2c18c8, 0x24c578),
}
ARM_WORDS = {0x24648c: 0xe7943003,  # original HFP callback GOT load
             0x2464ac: 0xe5853014,  # original HFP callback pointer storage
             0x2464b0: 0xeb00c558,  # original HFP RF registration
             0x24dc9c: 0xe5843010,  # original SIM callback pointer storage
             0x24dcac: 0xeb00a759}  # original SIM RF registration


def parse_relocations(s):
    result = {}
    for line in s.splitlines():
        f = line.strip().split()
        if len(f) < 5 or f[2] != 'R_ARM_GLOB_DAT':
            continue
        name = f[4].split('@')[0]
        if name not in RELOCS:
            continue
        if name in result:
            raise ValueError('duplicate callback relocation: ' + name)
        result[name] = (int(f[0], 16), int(f[3], 16))
    if result != RELOCS:
        raise ValueError('exact HFP/SIM callback relocation ABI mismatch: ' + repr(result))
    return result


def writable_load_contains(s, addr):
    for line in s.splitlines():
        f = line.strip().split()
        if len(f) >= 8 and f[0] == 'LOAD':
            start = int(f[2], 16)
            size = int(f[5], 16)
            flags = ' '.join(f[6:-1])
            if start <= addr < start + size and 'W' in flags:
                return True
    return False


def audit(raw, reloc_text, load_text):
    digest = hashlib.sha256(raw).hexdigest()
    if digest != OEM_SHA:
        raise ValueError('not the exact original user-supplied MU1005 btstack')
    if len(raw) < 0x1b97dc or raw[:6] != b'\x7fELF\x01\x01':
        raise ValueError('not ELF32 little-endian')
    if struct.unpack_from('<HH', raw, 16) != (2, 40):
        raise ValueError('not ARM ET_EXEC')
    got = parse_relocations(reloc_text)
    for name, (address, _) in got.items():
        if not writable_load_contains(load_text, address):
            raise ValueError('GOT relocation not in writable segment: ' + name)
    # First original executable PT_LOAD maps file +0x100000 to RVA. Any mismatch
    # forces fresh reverse engineering rather than moving an unverified hook.
    for va, expected in ARM_WORDS.items():
        offset = va - 0x100000
        if offset + 4 > len(raw) or struct.unpack_from('<I', raw, offset)[0] != expected:
            raise ValueError('original ARM code word mismatch at 0x%x' % va)
    return {'version': 'R97', 'scope': 'EXACT_OEM_STATIC_ONLY',
            'original_sha256': digest,
            'oem_elf': 'QNX6.5_ARM32_ET_EXEC',
            'original_got': {name: {'slot_va': '0x%x'%val[0], 'target_va': '0x%x'%val[1]}
                             for name, val in got.items()},
            'original_arm_words': {hex(k):hex(v) for k,v in ARM_WORDS.items()},
            'original_got_in_writable_segment': True,
            'original_hfp_callback_address_known': True,
            'original_iap2_callback_known': False,
            'real_original_hfp_callback_observed': False,
            'vehicle_verified': False, 'safe_to_flash': False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--btstack',required=True)
    p.add_argument('--out',required=True)
    a=p.parse_args()
    data=Path(a.btstack).read_bytes()
    r=subprocess.run(['readelf','-Wr',a.btstack],capture_output=True,text=True,check=True)
    l=subprocess.run(['readelf','-Wl',a.btstack],capture_output=True,text=True,check=True)
    try: report=audit(data,r.stdout,l.stdout)
    except ValueError as e:
        print('R97 exact OEM proof rejected:',e,file=sys.stderr)
        return 2
    Path(a.out).write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    print('R97 original HFP and SIM GLOB_DAT + ARM callsite + writable segment: VERIFIED (STATIC ONLY)')
    return 0

if __name__=='__main__': raise SystemExit(main())
