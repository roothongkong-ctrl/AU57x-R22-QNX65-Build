#!/usr/bin/env python3
"""Step 19: deterministic, DOCUMENT-ONLY SD review package; not a firmware installer.

Only accepts original R95 offline evidence. Missing genuine OEM runtime contracts
are **explicit NO-GO**; no --force, implicit authorization or executable payloads.
"""
import argparse
import hashlib
import json
from pathlib import Path
import stat
import sys
import zipfile

from sd_package_audit import FIRMWARE, inspect_zip

MANDATORY_MISSING = (
    'original_btstack_live_preload',
    'original_oem_rfcomm_callback_bound',
    'original_mfi_device_signature',
    'true_phone_iap2_link',
    'factory_supervisor_exact_config_path_and_hash',
    'production_quiesce_and_startup_owner',
    'car_boot_recovery_hook',
    'wifi_24ghz_phone_association',
    'original_airplay_dio_audio_video',
    'real_carplay_headunit_and_phone_test',
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check_input(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_nlink != 1:
        raise ValueError('unsafe evidence path')
    if path.stat().st_size > 256*1024:
        raise ValueError('evidence size limit')
    return path.read_bytes()


def make_status(r95_bytes):
    j=json.loads(r95_bytes)
    if (j.get('version') != 'R95' or j.get('baseline') != FIRMWARE or
        j.get('vehicle_verified') is not False or j.get('installable') is not False or
        j.get('safe_to_flash') is not False or len(j.get('steps',[])) != 15 or
        [s.get('step') for s in j['steps']] != list(range(3,18)) or
        any(s.get('offline_test') != 'PASS' or s.get('vehicle_complete') is not False for s in j['steps'])):
        raise ValueError('R95 provenance or offline evidence incorrect')
    return {
        'version':'R96', 'base':'R95', 'firmware':FIRMWARE,
        'r95_offline_evidence_sha256':digest(r95_bytes),
        'step18_offline_supervised_installer':'IMPLEMENTED_HOST_LAB_ONLY',
        'step19_review_candidate':'DOCUMENT_ONLY_NOT_INSTALLABLE',
        'step20_car_test':'NOT_ATTEMPTED',
        'unresolved_production_proofs':list(MANDATORY_MISSING),
        'steps_03_to_17_offline_test_count':15,
        'steps_03_to_17_vehicle_complete_count':0,
        'installable':False, 'safe_to_flash':False, 'vehicle_verified':False,
        'auto_install_in_car':False, 'can_boot_recover_on_car':False,
        'original_MFi_proved':False,
        'warning':'DO NOT insert this ZIP as a firmware update. NO GO for vehicle install.'}


def build(r95_evidence,out,docs):
    source=check_input(r95_evidence)
    status=make_status(source)
    out=Path(out)
    if out.exists() or out.is_symlink():
        raise ValueError('refusing to overwrite existing archive')
    docs=Path(docs)
    if docs.is_symlink() or not docs.is_dir():
        raise ValueError('unsafe docs directory')
    names={
      'payload/R96_STATUS.json':(json.dumps(status,ensure_ascii=False,indent=2,sort_keys=True)+'\n').encode(),
      'payload/R95_OFFLINE_EVIDENCE.json':source,
      'payload/READ_FIRST_CN.md':check_input(docs/'R96_READ_FIRST_CN.md'),
      'payload/R96_INSTALL_LIMITS_CN.md':check_input(docs/'R96_INSTALL_LIMITS_CN.md'),
    }
    manifest={
      'firmware':FIRMWARE,'release':'R96','review_only':True,
      'installable':False,'safe_to_flash':False,'vehicle_verified':False,
      'files':[{'path':p,'sha256':digest(data)} for p,data in sorted(names.items())],
      'important':'Nonexecuting source review; not a vehicle installation package.',
    }
    names['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2,sort_keys=True)+'\n').encode()
    out.parent.mkdir(parents=True,exist_ok=True)
    try:
        with zipfile.ZipFile(out,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for name in sorted(names):
                zi=zipfile.ZipInfo(name, date_time=(1980,1,1,0,0,0))
                zi.compress_type=zipfile.ZIP_DEFLATED
                zi.create_system=3
                zi.external_attr=(stat.S_IFREG|0o644)<<16
                z.writestr(zi,names[name],compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
        errs=inspect_zip(out)
        if errs:raise ValueError('output audit rejected: '+str(errs))
    except BaseException:
        if out.exists():out.unlink()
        raise
    return status


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--r95-evidence',required=True)
    p.add_argument('--docs',required=True)
    p.add_argument('--out',required=True)
    a=p.parse_args()
    try:
        s=build(a.r95_evidence,a.out,a.docs)
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError,zipfile.BadZipFile) as e:
        print('R96 REVIEW ONLY REJECT:',str(e),file=sys.stderr)
        return 2
    print(json.dumps({'archive':a.out,'installable':False,
      'missing_production_proofs':len(s['unresolved_production_proofs'])},ensure_ascii=False))
    return 0

if __name__=='__main__':
    raise SystemExit(main())
