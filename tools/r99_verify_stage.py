#!/usr/bin/env python3
"""Read-only verifier for the R99 factory iAP2 OFFLINE staging directory."""
import argparse
import json
from pathlib import Path
import sys
from r99_stock_iap2_route import (sha, stage_bluetooth_json,
                                  BLUETOOTH_SHA, IAP2_DRIVER_SHA, read_bounded)

EXPECTED = {
    'bluetooth.original.json', 'bluetooth.STAGED_REVIEW_ONLY.json',
    'iap2.original.cfg', 'iap2.STAGED_REVIEW_ONLY.cfg', 'R99_REVIEW_MANIFEST.json'
}

def verify(folder: Path, expected_bt: str | None = None, expected_iap2: str | None = None):
    if folder.is_symlink() or not folder.is_dir():
        raise ValueError('staging path must be regular directory')
    if {p.name for p in folder.iterdir()} != EXPECTED:
        raise ValueError('staged content names mismatch')
    for p in folder.iterdir():
        if p.is_symlink() or not p.is_file():
            raise ValueError('staging member must be regular file')
    manifest = json.loads(read_bounded(folder/'R99_REVIEW_MANIFEST.json'))
    if manifest.get('release') != 'R99' or manifest.get('route')!='OEM_NATIVE_IAP2_CONFIG_FIRST':
        raise ValueError('wrong manifest origin')
    if manifest.get('config_policy') != 'BT_JSON_ONLY_PRESERVE_IAP2_V2':
        raise ValueError('obsolete status-reporting staging policy; regenerate from originals')
    for required_false in ('installable','safe_to_flash','vehicle_verified','real_mfi_proven',
                           'stock_bt_rfcomm_transport_proven','stock_runtime_iap2_registration_proven'):
        if manifest.get(required_false) is not False:
            raise ValueError('review-only status must be explicitly false: '+required_false)
    if manifest.get('verified_oem_bluetooth',{}).get('sha256') != BLUETOOTH_SHA:
        raise ValueError('wrong original Bluetooth firmware fingerprint')
    if manifest.get('verified_oem_iap2_driver',{}).get('sha256') != IAP2_DRIVER_SHA:
        raise ValueError('wrong original iAP2 driver fingerprint')
    known=manifest.get('files')
    if not isinstance(known,dict) or set(known)!=EXPECTED-{'R99_REVIEW_MANIFEST.json'}:
        raise ValueError('unexpected staged file manifest')
    for name, digest in known.items():
        if sha(read_bounded(folder/name)) != digest:
            raise ValueError('staged file SHA256 mismatch: '+name)
    before_bt=read_bounded(folder/'bluetooth.original.json')
    before_cfg=read_bounded(folder/'iap2.original.cfg')
    if expected_bt and sha(before_bt) != expected_bt:
        raise ValueError('external Bluetooth original fingerprint mismatch')
    if expected_iap2 and sha(before_cfg) != expected_iap2:
        raise ValueError('external iAP2 original fingerprint mismatch')
    expected_bt_stage,_=stage_bluetooth_json(before_bt)
    expected_iap2_stage=before_cfg
    if expected_bt_stage!=read_bounded(folder/'bluetooth.STAGED_REVIEW_ONLY.json'):
        raise ValueError('Bluetooth config changed beyond permitted field')
    if expected_iap2_stage!=read_bounded(folder/'iap2.STAGED_REVIEW_ONLY.cfg'):
        raise ValueError('iAP2 config must be preserved byte-for-byte')
    return {'R99_STAGE_VERIFIED':True, 'no_install_payload':True,
            'factory_iap2_runtime_unproven':True,
            'bluetooth.original.sha256':sha(before_bt), 'iap2.original.sha256':sha(before_cfg)}

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('directory',type=Path)
    ap.add_argument('--original-bluetooth-sha256')
    ap.add_argument('--original-iap2-sha256')
    args=ap.parse_args()
    try:
        print(json.dumps(verify(args.directory,args.original_bluetooth_sha256,args.original_iap2_sha256),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError) as e:
        print('R99 DENY: '+str(e),file=sys.stderr);sys.exit(2)
