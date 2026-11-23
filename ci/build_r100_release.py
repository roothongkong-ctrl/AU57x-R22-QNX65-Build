#!/usr/bin/env python3
"""Deterministic SOURCE ONLY R100 distribution. Private OEM images never leave private dir."""
import hashlib
import json
import pathlib
import zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
BASE=pathlib.Path('/mnt/data/AU57x_R99_Factory_iAP2_DualConfig_Fallback_Source_ONLY_NO_INSTALL.zip')
OUT=pathlib.Path('/mnt/data/AU57x_R100_Factory_to_Graft_FailClosed_Source_ONLY_NO_INSTALL.zip')
FILES=[
 'README.md','R100_BUILD_INFO.txt','DO_NOT_INSTALL_R100.txt',
 'docs/R100_FACTORY_TO_GRAFT_CN.md',
 'project/source/legacy_mu1005/bluetooth/bt_route_gate.c',
 'project/source/legacy_mu1005/bluetooth/bt_route_gate.h',
 'project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.c',
 'project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.h',
 'tools/r100_mu1005_route_audit.py',
 'tests/test_r100_mu1005_route_audit.py',
 'tests/test_r100_route_gate.c',
 'ci/host_r100_route_gate.sh','ci/host_r100_route_audit.sh',
 'ci/host_r100_continuous.py',
 'ci/host_r95_source_compile.sh','ci/compile_qnx_in_image.sh',
 '.github/workflows/cloud12-review.yml','.github/workflows/qnx65-compile.yml',
 'ci/build_r100_release.py',
 'r100_result/R100_ORIGINAL_MU1005_ROUTE_AUDIT.json',
 'r100_result/R100_CONTINUOUS_RESULTS.json',
]
FILES.extend('r100_result/'+n for n in sorted(p.name for p in (ROOT/'r100_result').glob('*.log')))

def build():
    if OUT.exists(): raise ValueError('refuse to overwrite an existing R100 archive')
    with zipfile.ZipFile(BASE) as z:
        data={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
    for n in FILES:
        p=ROOT/n
        if not p.is_file() or p.is_symlink(): raise ValueError('required file missing/linked '+n)
        data[n]=p.read_bytes()
    # R96 carried a nested review-only ZIP. R100 keeps its sources, but no nested ZIP.
    data.pop('r96_result/AU57x_R96_SD_REVIEW_NOT_INSTALL.zip',None)
    data.pop('RELEASE_MANIFEST_R100.json',None)
    data.pop('SHA256SUMS_R100.txt',None)
    for n in data:
        if n.startswith('/') or '..' in pathlib.PurePosixPath(n).parts:
            raise ValueError('unsafe path '+n)
        if any(k in n.lower() for k in ('private_oem','/btstack.elf','/bluetooth.elf','original_bluetooth.elf',
                                      'ipod-drvr-iap2.so','connectivity_launcher','mfi_private')):
            raise ValueError('OEM file leak '+n)
        if n.lower().endswith(('.elf','.so','.7z','.img','.bin','.key','.pem','.p12','.pfx','.spk','.zip')):
            raise ValueError('NO INSTALL package may not have binaries '+n)
    report=json.loads(data['r100_result/R100_CONTINUOUS_RESULTS.json'])
    if report['passed']!=report['total'] or report['total']!=12:
        raise ValueError('independent offline test matrix failed')
    oem=json.loads(data['r100_result/R100_ORIGINAL_MU1005_ROUTE_AUDIT.json'])
    if oem['route_eligible_for_auto_activation'] is not False or oem['installable'] is not False:
        raise ValueError('OEM audit must refuse release')
    manifest={'release':'R100','source_only':True,'installable':False,
              'safe_to_flash':False,'vehicle_verified':False,
              'factory_iap2_live':False,'graft_iap2_registration_live':False,
              'features':['pinned OEM binary static route triage',
                          'C factory/graft fail-closed guard at real QNX native attach',
                          '12 regression suites and 44 source C modules'],
              'members':{n:hashlib.sha256(v).hexdigest() for n,v in sorted(data.items())}}
    data['RELEASE_MANIFEST_R100.json']=(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n').encode()
    data['SHA256SUMS_R100.txt']=''.join(f'{hashlib.sha256(v).hexdigest()}  {n}\n' for n,v in sorted(data.items())).encode()
    with zipfile.ZipFile(OUT,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for n,v in sorted(data.items()):
            info=zipfile.ZipInfo(n,date_time=(2026,11,23,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=((0o100755 if n.endswith('.sh') else 0o100644)<<16)
            z.writestr(info,v)
    with zipfile.ZipFile(OUT) as z:
        if z.testzip() is not None or len(z.namelist())!=len(set(z.namelist())):
            raise ValueError('corrupt ZIP')
        m=json.loads(z.read('RELEASE_MANIFEST_R100.json'))
        if len(z.namelist())!=len(m['members'])+2:raise ValueError('bad ZIP member count')
        for n,d in m['members'].items():
            if hashlib.sha256(z.read(n)).hexdigest()!=d:raise ValueError('SHA mismatch '+n)
    print(json.dumps({'path':str(OUT),'sha256':hashlib.sha256(OUT.read_bytes()).hexdigest(),
                      'size_bytes':OUT.stat().st_size,'files':len(data),
                      'installable':False},indent=2))
if __name__=='__main__':build()
