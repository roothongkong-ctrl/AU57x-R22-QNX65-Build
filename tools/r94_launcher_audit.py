#!/usr/bin/env python3
"""Static evidence for exact user-owned MU1005 connectivity_launcher.
Does not execute original ELF, claim LD_PRELOAD support, or modify firmware.
"""
import argparse
import hashlib
import json
from pathlib import Path

OEM_SHA = '08896149dcdcc2bc5a6b22ce78682ad5949fbeb8fdc0fb1249162861e40914b8'
NATIVE_IMPORTS = (
    b'_ZN4osal10ProcessTNG13setExecutableERKN3ipl12basic_string',
    b'connectivity.json',
    b'm_JSONReader',
    b'StartModule',
    b'connectivity_launcher',
)


def audit(data, expected_sha=OEM_SHA):
    if len(data) < 52 or data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 1:
        raise ValueError('not QNX-style ELF32 little-endian')
    if int.from_bytes(data[18:20], 'little') != 40:
        raise ValueError('not ARM e_machine=40')
    if int.from_bytes(data[16:18], 'little') != 2:
        raise ValueError('not executable ET_EXEC')
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected_sha:
        raise ValueError('not pinned exact MU1005 connectivity_launcher')
    missing = [needle.decode() for needle in NATIVE_IMPORTS if needle not in data]
    if missing:
        raise ValueError('missing required launcher strings/imports: '+repr(missing))
    return {
        'stock_executable_sha256': digest,
        'elf_class': 'ELF32', 'machine': 'ARM', 'kind': 'ET_EXEC',
        'has_osal_ProcessTNG_setExecutable_reference': True,
        'has_connectivity_json_reference': True,
        'has_JSONReader_reference': True,
        'has_StartModule_reference': True,
        'original_launch_manager_process_mechanism': 'dynamic OSAL ProcessTNG / JSON config',
        'proves_config_can_accept_our_wrapper': False,
        'proves_supervisor_inherits_LD_PRELOAD': False,
        'proves_real_btstack_loader': False,
        'owner_callback_bound': False,
        'scope': 'exact-OEM-static-proof; never a runtime test',
    }


def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('--elf', required=True)
    a.add_argument('--out')
    args = a.parse_args()
    try:
        evidence = audit(Path(args.elf).read_bytes())
    except (OSError, ValueError) as err:
        a.exit(2, f'R94 OEM launcher DENIED: {err}\n')
    data = json.dumps(evidence, indent=2) + '\n'
    if args.out: Path(args.out).write_text(data)
    print(data)


if __name__ == '__main__': main()
