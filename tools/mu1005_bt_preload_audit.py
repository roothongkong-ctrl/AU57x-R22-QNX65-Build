#!/usr/bin/env python3
"""R92 private offline MU1005 btstack/LD_PRELOAD plausibility audit.
Produces only hashes, entry offsets, and evidence, no stock ELF content.
"""
from __future__ import annotations
import argparse, hashlib, json, re, subprocess, struct
from pathlib import Path

STOCK_SHA = '51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566'
CALLS = (0x236668, 0x2368c8, 0x236c2c, 0x236c48)
PLT = 0x117390

def header_ok(data: bytes) -> bool:
    return (len(data) >= 52 and data[:4] == b'\x7fELF' and data[4] == 1
            and data[5] == 1 and struct.unpack_from('<H', data, 16)[0] == 2
            and struct.unpack_from('<H', data, 18)[0] == 40)

def inspect_asm(text: str):
    lines = text.splitlines()
    found = []
    for ln in lines:
        m = re.match(r'^\s*([0-9a-fA-F]+):.*\bbl\s+0x([0-9a-fA-F]+)\b', ln)
        if not m:
            continue
        src, dst = (int(m.group(1), 16), int(m.group(2), 16))
        if dst == PLT:
            found.append(src)
    return sorted(found)

def audit(path: Path) -> dict:
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    ok = header_ok(data)
    if digest != STOCK_SHA or not ok:
        raise ValueError('original MU1005 QNX ARM32 btstack hash/ELF mismatch')
    syms = subprocess.run(['readelf', '--dyn-syms', '--wide', str(path)], check=True, text=True, capture_output=True).stdout
    if not re.search(r'\bUND\s+pthread_create\b', syms):
        raise ValueError('original imported pthread_create not present')
    asm = subprocess.run(['llvm-objdump', '-d', '--no-show-raw-insn', str(path)], check=True, text=True, capture_output=True).stdout
    found = inspect_asm(asm)
    if found != list(CALLS):
        raise ValueError(f'original pthread_create PLT callsites mismatch: {found!r}')
    return {'firmware': 'MHI2_CN_AU57x_K1006_2/MU1005', 'btstack_sha256': digest,
            'elf32_arm_qnx': ok, 'pthread_create_plt_va': hex(PLT),
            'original_pthread_create_callsites': [hex(a) for a in found],
            'preload_supported_by_qnx65_docs': True,
            'runtime_in_car_observed': False, 'real_rfcomm_hook_observed': False,
            'iap2_mfi_vehicle_verified': False, 'safe_to_flash': False}

def main():
    p = argparse.ArgumentParser();p.add_argument('--btstack', required=True);p.add_argument('--out');a=p.parse_args()
    result = audit(Path(a.btstack))
    if a.out: Path(a.out).write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(result, indent=2))
if __name__ == '__main__': main()
