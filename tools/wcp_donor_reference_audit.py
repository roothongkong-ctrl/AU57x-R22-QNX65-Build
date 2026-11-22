#!/usr/bin/env python3
"""Read-only reference manifest for congobg's MHI2Q *different firmware*.

Inspects the user-supplied ZIP without extracting or executing any item.
Proves only pack structure / QNX ELF architecture, not AU57x compatibility.
Never includes any donor bytes in its JSON output.
"""
import argparse
import hashlib
import io
import json
import struct
import tarfile
import zipfile
from pathlib import PurePosixPath

WATCH = ('libbthook.so', 'libwcpstart.so', 'wcpd', 'wcppaird',
         'wcp-boot.sh', 'dio-up.sh', 'start-ap.sh', 'WCP.jar')

def audit(path):
    result = {'reference': 'congobg/MHI2Q_WiFi_CarPlay',
              'reference_platform': 'MHI2Q 3638 MU0917 (not AU57x)',
              'target_platform': 'MHI2_CN_AU57x_K1006_2 MU1005',
              'safe_to_flash': False, 'donor_binaries_may_be_installed': False,
              'vendor_code_embedded': False,
              'components': {}}
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        tarpaths = [x for x in names if x.endswith('/MHI2QC_3638/wcp.tar.gz')]
        if len(tarpaths) != 1:
            raise ValueError('expected exactly one MHI2Q 3638 donor payload')
        if z.getinfo(tarpaths[0]).file_size > 8_000_000:
            raise ValueError('donor compressed payload too large')
        data = z.read(tarpaths[0])
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as t:
        members = t.getmembers()
        if len(members) > 300:
            raise ValueError('unexpected donor member count')
        for m in members:
            n = m.name.removeprefix('./')
            p = PurePosixPath(n)
            if p.is_absolute() or '..' in p.parts or m.issym() or m.islnk() or m.isdev():
                raise ValueError('unsafe donor archive entry')
            if not m.isfile():
                continue
            if m.size > 3_000_000:
                raise ValueError('unexpected donor entry size')
            basename = p.name
            if basename not in WATCH:
                continue
            if basename in result['components']:
                raise ValueError('duplicate donor component')
            content = t.extractfile(m).read()
            elf32_arm = len(content) > 52 and content[:5] == b'\x7fELF\x01' and \
                content[5] == 1 and struct.unpack_from('<H', content, 18)[0] == 40
            result['components'][basename] = {
                'source_path': n, 'size_bytes': len(content),
                'sha256': hashlib.sha256(content).hexdigest(),
                'elf32_arm': bool(elf32_arm),
                'classification': 'donor_reference_ONLY',
            }
    if set(result['components']) != set(WATCH):
        raise ValueError('required donor references missing')
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--donor-zip', required=True)
    p.add_argument('--out', required=True)
    args = p.parse_args()
    with open(args.out, 'w', encoding='utf-8') as out:
        json.dump(audit(args.donor_zip), out, ensure_ascii=False, indent=2)
        out.write('\n')
    print('R87 DONOR REFERENCE AUDIT PASS (SOURCE ONLY / NO INSTALL)')
if __name__ == '__main__':
    main()
