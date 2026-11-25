#!/usr/bin/env python3
"""Strict, reversible OFFLINE staging of original MU1005 iAP2 configuration.

Neither this tool nor its output is a vehicle installer. Unlike the experimental
HFP GOT path, this route tries *existing* OEM bluetooth + iAP2 driver support,
but their runtime service and bridge are still unproven.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

BLUETOOTH_SHA = '7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272'
IAP2_DRIVER_SHA = '42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf'
OEM_STRING_MARKERS = {
    'bluetooth': [b'bluetooth.enableIap\x00', b'SERVICETYPE_IAP2\x00', b'iapEnabled\x00'],
    'iap2_driver': [b'BluetoothiAP2Link\x00', b'BluetoothTransportComponent\x00',
                    b'WiFiTransportComponent\x00']
}
MAX_FILE = 1 << 20


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def read_bounded(path: Path, max_bytes: int = MAX_FILE) -> bytes:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > max_bytes:
        raise ValueError(f'not a regular bounded input: {path}')
    return path.read_bytes()


def audit_binary(path: Path, expected: str, markers: list[bytes]) -> dict:
    data = read_bounded(path, 3 << 20)
    if sha(data) != expected:
        raise ValueError(f'incorrect original OEM binary hash: {path.name}')
    if not data.startswith(b'\x7fELF\x01\x01') or data[18:20] != b'\x28\x00':
        raise ValueError('expected original QNX ELF32 little-endian ARM')
    missing = [s.decode('ascii', errors='replace') for s in markers if s not in data]
    if missing:
        raise ValueError(f'original stock evidence missing: {missing}')
    return {'sha256': sha(data), 'elf': 'ELF32_LE_ARM',
            'stock_markers_present': [s.decode('ascii').strip('\0') for s in markers]}


def no_duplicates(pairs):
    obj = {}
    for k, v in pairs:
        if k in obj:
            raise ValueError(f'duplicate JSON key {k!r}')
        obj[k] = v
    return obj


def stage_bluetooth_json(raw: bytes) -> tuple[bytes, dict]:
    if len(raw) > MAX_FILE or b'\x00' in raw:
        raise ValueError('invalid original Bluetooth config')
    try:
        config = json.loads(raw.decode('utf-8'), object_pairs_hook=no_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError('Bluetooth config must be valid UTF-8 JSON') from exc
    if not isinstance(config, dict):
        raise ValueError('Bluetooth config root must be object')
    candidates = []
    if 'bluetooth.enableIap' in config:
        candidates.append((config, 'bluetooth.enableIap'))
    if 'enableIap' in config and 'topologyLogic' in config:
        candidates.append((config, 'enableIap'))
    if isinstance(config.get('bluetooth'), dict) and 'enableIap' in config['bluetooth']:
        candidates.append((config['bluetooth'], 'enableIap'))
    if len(candidates) != 1:
        raise ValueError('unknown or ambiguous OEM enableIap JSON schema; refuse to guess')
    node, key = candidates[0]
    old = node[key]
    if type(old) is not bool:
        raise ValueError('enableIap must be JSON boolean, not number/string')
    if old:
        raise ValueError('original enableIap is already true; no change to generate')
    node[key] = True
    updated = (json.dumps(config, ensure_ascii=False, indent=2) + '\n').encode('utf8')
    # Ensure no semantics changed except exactly one boolean.
    check = json.loads(updated, object_pairs_hook=no_duplicates)
    checknode = check if key == 'bluetooth.enableIap' or (key == 'enableIap' and 'topologyLogic' in check) else check['bluetooth']
    checknode[key] = False
    if check != json.loads(raw.decode('utf8'), object_pairs_hook=no_duplicates):
        raise ValueError('unexpected sibling config mutation')
    return updated, {'selected_key': 'bluetooth.enableIap' if key == 'bluetooth.enableIap' else
                     ('enableIap' if node is config else 'bluetooth.enableIap [nested]'),
                     'before': False, 'after': True}


def stage_iap2_ini(raw: bytes) -> tuple[bytes, dict]:
    """Enable only a complete, already-present commented Bluetooth transport.

    The original MU1005 template contains enable/id/name/connectstatus/mac.
    Appending a bare [bluetooth] enable=1 section is unsafe: it silently drops
    the transport identity and link parameters. Unknown/incomplete templates
    are rejected rather than populated with guessed values.
    """
    import re
    if len(raw) > MAX_FILE or b'\x00' in raw:
        raise ValueError('invalid original iap2.cfg')
    try:
        text = raw.decode('utf8')
    except UnicodeError as e:
        raise ValueError('iap2.cfg must be UTF-8') from e
    if '\r' in text and '\r\n' not in text:
        raise ValueError('unsupported CR-only line endings')
    eol = '\r\n' if '\r\n' in text else '\n'
    if '\r\n' in text and '\n' in text.replace('\r\n', ''):
        raise ValueError('mixed line endings not accepted')
    lines = text.splitlines(keepends=True)
    active_sections = []
    commented_sections = []
    current = None
    active_enable = []
    for n, line in enumerate(lines):
        s = line.strip()
        if s.startswith('[') and s.endswith(']'):
            current = s[1:-1].strip().lower()
            if current == 'bluetooth':
                active_sections.append(n)
            continue
        if s.startswith('#[') and s.endswith(']'):
            section = s[2:-1].strip().lower()
            if section == 'bluetooth':
                commented_sections.append(n)
            continue
        if s.startswith((';', '#')) or not s:
            continue
        if current == 'bluetooth' and '=' in s and s.split('=', 1)[0].strip().lower() == 'enable':
            active_enable.append((n, s.split('=', 1)[1].strip()))
    if len(active_sections) > 1 or len(active_enable) > 1:
        raise ValueError('duplicate iAP2 Bluetooth section or enable setting')
    if active_sections:
        if len(active_enable) != 1:
            raise ValueError('active Bluetooth section has no unique enable value')
        index, value = active_enable[0]
        if value not in ('0', 'false', 'False', 'off', 'OFF'):
            raise ValueError('Bluetooth enable value unknown or already true')
        old_line = lines[index]
        line_no_eol = old_line.rstrip('\r\n')
        indent = line_no_eol[:len(line_no_eol)-len(line_no_eol.lstrip())]
        lines[index] = indent + 'enable = 1' + (eol if old_line.endswith('\n') else '')
        return ''.join(lines).encode('utf8'), {
            'style': 'existing active section', 'before': value, 'after': '1'
        }
    if len(commented_sections) != 1:
        raise ValueError('no unique Bluetooth section/template; cannot enable safely')

    section_start = commented_sections[0]
    section_end = len(lines)
    for n in range(section_start + 1, len(lines)):
        s = lines[n].strip()
        if s.startswith('[') and s.endswith(']'):
            section_end = n
            break
    required = ('enable', 'id', 'name', 'connectstatus', 'mac')
    found = {key: [] for key in required}
    parsed = {}
    for n in range(section_start + 1, section_end):
        line = lines[n]
        match = re.match(r'^(\s*)#\s*([A-Za-z][A-Za-z0-9_]*)\s*=\s*(.*?)\s*(\r?\n)?$', line)
        if not match:
            continue
        key = match.group(2).lower()
        if key in found:
            found[key].append((n, match))
            parsed[key] = match.group(3).strip()
    missing = [key for key, values in found.items() if not values]
    duplicate = [key for key, values in found.items() if len(values) > 1]
    if missing:
        raise ValueError('commented Bluetooth template missing required fields: ' + ','.join(missing))
    if duplicate:
        raise ValueError('duplicate commented Bluetooth fields: ' + ','.join(duplicate))
    if parsed['enable'].lower() not in ('yes', 'true', '1', 'on', 'enable'):
        raise ValueError('commented Bluetooth enable value is not enabled; refusing to infer desired value')
    try:
        transport_id = int(parsed['id'], 10)
    except ValueError as exc:
        raise ValueError('commented Bluetooth transport id must be numeric') from exc
    if not 1 <= transport_id <= 65535:
        raise ValueError('commented Bluetooth transport id out of range')
    if not parsed['name']:
        raise ValueError('commented Bluetooth transport name is empty')
    if parsed['connectstatus'].lower() not in ('enable', 'yes', 'true', '1', 'on'):
        raise ValueError('commented Bluetooth connectstatus is not enabled')
    if not re.fullmatch(r'(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}', parsed['mac']):
        raise ValueError('commented Bluetooth MAC is not a six-octet address')

    # Activate exactly the existing template's known fields; keep all other
    # comments and sections byte-for-byte. Do not synthesize IDs or MACs.
    staged = list(lines)
    header = lines[section_start]
    newline = '\r\n' if header.endswith('\r\n') else ('\n' if header.endswith('\n') else '')
    indent = header[:len(header)-len(header.lstrip())]
    staged[section_start] = indent + '[bluetooth]' + newline
    for key in required:
        n, match = found[key][0]
        old = lines[n]
        line_eol = '\r\n' if old.endswith('\r\n') else ('\n' if old.endswith('\n') else '')
        body = match.group(3).strip()
        if key == 'enable':
            body = 'yes'
        staged[n] = match.group(1) + key + ' = ' + body + line_eol
    updated = ''.join(staged).encode('utf8')
    return updated, {
        'style': 'uncommented complete existing Bluetooth template',
        'before': 'commented', 'after': 'enabled',
        'transport_id': transport_id,
        'transport_name': parsed['name'],
        'fields_activated': list(required)
    }

def assert_hash(data: bytes, value: str | None, kind: str):
    if not value or len(value) != 64 or not all(c in '0123456789abcdef' for c in value):
        raise ValueError(f'{kind} exact original SHA256 must be supplied')
    if sha(data) != value:
        raise ValueError(f'{kind} original hash mismatch')


def prepare(bt_config_path: Path, iap2_path: Path, original_bt_hash: str, original_iap2_hash: str,
            oem_bt_path: Path, oem_driver_path: Path, out: Path):
    if out.exists() or out.is_symlink():
        raise ValueError('output directory exists; no overwrite')
    orig_bt = read_bounded(bt_config_path)
    orig_iap2 = read_bounded(iap2_path)
    assert_hash(orig_bt, original_bt_hash, 'Bluetooth JSON')
    assert_hash(orig_iap2, original_iap2_hash, 'iap2.cfg')
    verified_bt = audit_binary(oem_bt_path, BLUETOOTH_SHA, OEM_STRING_MARKERS['bluetooth'])
    verified_iap2 = audit_binary(oem_driver_path, IAP2_DRIVER_SHA, OEM_STRING_MARKERS['iap2_driver'])
    new_bt, info_bt = stage_bluetooth_json(orig_bt)
    new_iap2, info_iap2 = stage_iap2_ini(orig_iap2)
    assert new_bt != orig_bt and new_iap2 != orig_iap2
    manifest = {
        'release': 'R99', 'route': 'OEM_NATIVE_IAP2_CONFIG_FIRST',
        'original_platform': 'MHI2_CN_AU57x_K1006_2/MU1005',
        'verified_oem_bluetooth': verified_bt,
        'verified_oem_iap2_driver': verified_iap2,
        'changes': {'bluetooth':info_bt, 'iap2_driver':info_iap2},
        'files': {
            'bluetooth.original.json': sha(orig_bt),
            'bluetooth.STAGED_REVIEW_ONLY.json': sha(new_bt),
            'iap2.original.cfg': sha(orig_iap2),
            'iap2.STAGED_REVIEW_ONLY.cfg': sha(new_iap2),
        },
        'rollback': 'Restore the two ORIGINAL byte-exact files, with their original permissions and ownership; do not mix versions.',
        'experimental_r98_btstack_injection_required': False,
        'stock_runtime_iap2_registration_proven': False,
        'stock_bt_rfcomm_transport_proven': False,
        'real_mfi_proven': False,
        'wireless_airplay_proven': False,
        'vehicle_verified': False,
        'installable': False,
        'safe_to_flash': False,
        'result': 'TWO_CONFIG_OFFLINE_STAGING_DONE__RUNTIME_NO_GO',
    }
    out.parent.mkdir(parents=True,exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix='R99_private_stage_',dir=out.parent))
    try:
        for name, content in (
            ('bluetooth.original.json',orig_bt),
            ('bluetooth.STAGED_REVIEW_ONLY.json',new_bt),
            ('iap2.original.cfg',orig_iap2),
            ('iap2.STAGED_REVIEW_ONLY.cfg',new_iap2)):
            f=temp/name
            with f.open('xb') as h:
                h.write(content)
                h.flush()
                os.fsync(h.fileno())
            os.chmod(f,0o600)
        (temp/'R99_REVIEW_MANIFEST.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
        os.chmod(temp/'R99_REVIEW_MANIFEST.json',0o600)
        temp.rename(out)
    finally:
        if temp.exists(): shutil.rmtree(temp)
    return manifest


def cli(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bluetooth-config',type=Path,required=True)
    p.add_argument('--iap2-cfg',type=Path,required=True)
    p.add_argument('--bluetooth-sha256',required=True)
    p.add_argument('--iap2-sha256',required=True)
    p.add_argument('--original-bluetooth-elf',type=Path,required=True)
    p.add_argument('--original-iap2-driver',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args(argv)
    try:
        result=prepare(a.bluetooth_config,a.iap2_cfg,a.bluetooth_sha256,a.iap2_sha256,
                       a.original_bluetooth_elf,a.original_iap2_driver,a.out)
    except (ValueError,OSError) as exc:
        print('R99 DENY:',str(exc),file=sys.stderr);return 2
    print(json.dumps(result,indent=2,ensure_ascii=False));return 0

if __name__=='__main__':raise SystemExit(cli())
