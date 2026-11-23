#!/usr/bin/env python3
"""Deterministic, verified offline source-only R98; no OEM files or executable installer."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]
DST=Path('/mnt/data/AU57x_R98_MU1005_IAP2_Stock_Owner_Handoff_Source_ONLY_NO_INSTALL.zip')
DIRS=('project','tools','ci','tests','docs','.github','R93_SD_MIB_ROOT')
FILES=('README.md','LICENSE','NOTICE_THIRD_PARTY_CN.md',
       'R95_BUILD_INFO.txt','DO_NOT_INSTALL_R95.txt',
       'R96_BUILD_INFO.txt','DO_NOT_INSTALL_R96.txt',
       'R97_BUILD_INFO.txt','DO_NOT_INSTALL_R97.txt',
       'R98_BUILD_INFO.txt','DO_NOT_INSTALL_R98.txt',
       'r95_result/R95_STEPS_03_TO_17_EVIDENCE.json',
       'r96_result/R96_STAGE18_19_EVIDENCE.json',
       'r97_result/R97_NATIVE_OWNER_AND_HFP_EVIDENCE.json',
       'r98_result/R98_NATIVE_IAP2_HANDOFF_EVIDENCE.json',
       'r97_result/R97_PINNED_OEM_HFP_GOT.json',
       'r96_result/AU57x_R96_SD_REVIEW_NOT_INSTALL.zip')
EXCLUDE=('__pycache__','.pyc','.so','.o','.elf','.img','.bin','.pem','.key','.p12','.pfx',
         '.7z','.spk','qnx_build_result','cloud12_result','r95_result','r96_result','r97_result','r98_result',
         '/out/','graft_au57x_candidate.c')
BANNED=('.so','.o','.elf','.bin','.img','.dll','.7z','.spk','.pem','.key','.p12','.pfx')
files=[]
for dirname in DIRS:
    for f in (ROOT/dirname).rglob('*'):
        if not f.is_file() or f.is_symlink():continue
        path=f.relative_to(ROOT).as_posix()
        if any(x in path for x in EXCLUDE):continue
        if f.stat().st_size>1_000_000:continue
        files.append(f)
for rel in FILES:
    f=ROOT/rel
    if not f.is_file() or f.is_symlink():raise FileNotFoundError(rel)
    files.append(f)
files=sorted(set(files),key=lambda f:f.relative_to(ROOT).as_posix())
rows=[]
for f in files:
    path=f.relative_to(ROOT).as_posix();data=f.read_bytes()
    if path.lower().endswith(BANNED):raise ValueError('forbidden binary/key: '+path)
    if path.endswith('.zip') and path!='r96_result/AU57x_R96_SD_REVIEW_NOT_INSTALL.zip':
        raise ValueError('nested unauthorized archive '+path)
    rows.append({'path':path,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
report={
    'version':'R98','baseline':'MHI2_CN_AU57x_K1006_2/MU1005',
    'scope':'SOURCE_ONLY_REAL_TCP_IAP2_OWNER_TO_NATIVE_R45_HANDOFF',
    'installable':False,'safe_to_flash':False,'vehicle_verified':False,
    'genuine_apple_mfi_verified':False,'iap2_registered_on_original_btstack':False,
    'original_hfp_callback_observed_on_vehicle':False,
    'real_native_owner_connected':False,
    'native_owner_handoff_adapter':'QNX_SOURCE_IMPLEMENTED_WITH_EXTERNAL_OWNER_PROOF',
    'stock_binary_included':False,'includes_keys':False,
    'oem_static_got_sha256':'51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566',
    'qnx_native_compile':'PENDING_GITHUB_R98',
    'original_ownership_callback':'STILL_UNBOUND_ORIGINAL_IAP2_ROLE_UNKNOWN',
    'source_c_modules':43,'host_suites':10,
    'files':rows}
mp=ROOT/'RELEASE_MANIFEST_R98.json';mp.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
sh=ROOT/'SHA256SUMS_R98.txt';sh.write_text(''.join(f"{r['sha256']}  {r['path']}\n" for r in rows))
entries=sorted(files+[mp,sh],key=lambda f:f.relative_to(ROOT).as_posix())
with zipfile.ZipFile(DST,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for f in entries:
        rel=f.relative_to(ROOT).as_posix();info=zipfile.ZipInfo(rel,date_time=(2026,11,23,0,0,0))
        info.compress_type=zipfile.ZIP_DEFLATED;info.create_system=3
        info.external_attr=0o100644<<16
        z.writestr(info,f.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
with zipfile.ZipFile(DST) as z:
    assert z.testzip() is None
    assert len(z.namelist())==len(set(z.namelist()))==len(entries)
    for row in rows:
        assert hashlib.sha256(z.read(row['path'])).hexdigest()==row['sha256']
print(json.dumps({'path':str(DST),'files':len(entries),'bytes':DST.stat().st_size,
    'sha256':hashlib.sha256(DST.read_bytes()).hexdigest(),'no_install':True}))
