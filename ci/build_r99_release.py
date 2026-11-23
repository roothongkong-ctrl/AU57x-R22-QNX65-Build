#!/usr/bin/env python3
"""Reproducible R99 source-only ZIP, based on R98; no OEM/private ELF."""
import hashlib,json,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=Path('/mnt/data/AU57x_R98_MU1005_IAP2_Stock_Owner_Handoff_Source_ONLY_NO_INSTALL.zip')
NAME='AU57x_R99_Factory_iAP2_DualConfig_Fallback_Source_ONLY_NO_INSTALL.zip'
OUTPUT=Path('/mnt/data')/NAME
UPDATE=[
'README.md','R99_BUILD_INFO.txt','DO_NOT_INSTALL_R99.txt',
'docs/R99_FACTORY_IAP2_ROUTE_CN.md','tools/r99_stock_iap2_route.py',
 'tools/r99_verify_stage.py','tests/test_r99_stock_iap2_route.py',
 'tests/test_r99_verify_stage.py',
 'tests/fixtures/R99_DEMO_NOT_OEM_bluetooth.json',
 'tests/fixtures/R99_DEMO_NOT_OEM_iap2.cfg',
 'ci/host_r99_stock_iap2.sh','ci/build_r99_release.py',
 '.github/workflows/cloud12-review.yml',
 'r99_result/R99_DEMO_AUDIT.json',
 'r99_result/R99_DEMO_VERIFICATION.json',
 'r99_result/R99_HOST_TEST.log',
 'r99_result/R99_FINAL_STATUS.json']
DEMO=[
'bluetooth.original.json','bluetooth.STAGED_REVIEW_ONLY.json',
'iap2.original.cfg','iap2.STAGED_REVIEW_ONLY.cfg','R99_REVIEW_MANIFEST.json']
UPDATE += ['r99_result/R99_DEMO_NOT_OEM_STAGING/'+x for x in DEMO]


def create():
    if OUTPUT.exists(): raise RuntimeError('R99 release exists; no silent overwrite')
    with zipfile.ZipFile(BASE) as z:
        data={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
    for p in UPDATE:
        f=ROOT/p
        if not f.is_file() or f.is_symlink():raise RuntimeError('missing/ambiguous '+p)
        data[p]=f.read_bytes()
    # No confidential proprietary files inside the release.
    for n in data:
        if any(t in n.lower() for t in ('private_oem','original_btstack.elf','original_bluetooth.elf',
                                       'ipod-drvr-iap2.so','connectivity_launcher','/auth_cert', 'mfi_private')):
            raise RuntimeError('unsafe OEM member '+n)
    data.pop('RELEASE_MANIFEST_R99.json',None)
    data.pop('SHA256SUMS_R99.txt',None)
    manifest={
        'release':'R99','source_only':True,'installable':False,
        'safe_to_flash':False,'vehicle_verified':False,
        'scope':'OEM_NATIVE_IAP2_TWO_CONFIG_OFFLINE_STAGE',
        'factory_iap2_runtime_proven':False,
        'sample_cfg_is_real_vehicle':False,
        'test_count':23,
        'members':{n:hashlib.sha256(b).hexdigest() for n,b in sorted(data.items())}}
    data['RELEASE_MANIFEST_R99.json']=(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n').encode('utf8')
    data['SHA256SUMS_R99.txt']=''.join(f'{hashlib.sha256(b).hexdigest()}  {n}\n' for n,b in sorted(data.items())).encode('utf8')
    with zipfile.ZipFile(OUTPUT,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for n,b in sorted(data.items()):
            info=zipfile.ZipInfo(n,date_time=(2026,11,23,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=((0o100755 if n.endswith('.sh') else 0o100644)<<16)
            z.writestr(info,b)
    with zipfile.ZipFile(OUTPUT) as z:
        if z.testzip() is not None or len(z.namelist())!=len(set(z.namelist())):
            raise RuntimeError('ZIP integrity failure')
        recorded=json.loads(z.read('RELEASE_MANIFEST_R99.json'))
        assert all(hashlib.sha256(z.read(n)).hexdigest()==v for n,v in recorded['members'].items())
        assert len(z.namelist())==len(recorded['members'])+2
    print(json.dumps({'release':str(OUTPUT),'size':OUTPUT.stat().st_size,
                      'sha256':hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
                      'members':len(data),'source_only':True,'installable':False},indent=2))
if __name__=='__main__':create()
