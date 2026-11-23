#!/usr/bin/env python3
"""Deterministic source-only R96 distribution with exact SHA256 per file.

Never packages original MU1005 OEM ELF, compiled binaries, Apple MFi keys or
any runnable SD firmware-flash installer. The separate SD ZIP is doc-only.
"""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]
DST=Path('/mnt/data/AU57x_R96_Steps18-19_Offline_OneClick_Review_Source_ONLY_NO_INSTALL.zip')
INCLUDE_DIRS=('project','tools','ci','tests','docs','.github','R93_SD_MIB_ROOT')
INCLUDE_FILES=('README.md','LICENSE','NOTICE_THIRD_PARTY_CN.md',
 'R95_BUILD_INFO.txt','DO_NOT_INSTALL_R95.txt','R96_BUILD_INFO.txt','DO_NOT_INSTALL_R96.txt',
 'r95_result/R95_STEPS_03_TO_17_EVIDENCE.json',
 'r96_result/R96_STAGE18_19_EVIDENCE.json',
 'r96_result/AU57x_R96_SD_REVIEW_NOT_INSTALL.zip')
EXCLUDE=('__pycache__','.pyc','.so','.o','.elf','.img','.bin','.pem','.key','.p12','.pfx',
         '.7z','.spk','qnx_build_result','cloud12_result','r95_result','r96_result',
         '/out/','graft_au57x_candidate.c')
REJECT_SUFFIXES=('.so','.o','.elf','.bin','.img','.dll','.7z','.spk','.pem','.key','.p12','.pfx')
files=[]
for dirname in INCLUDE_DIRS:
  for f in (ROOT/dirname).rglob('*'):
    if not f.is_file() or f.is_symlink():continue
    rel=f.relative_to(ROOT).as_posix()
    if any(x in rel for x in EXCLUDE):continue
    if f.stat().st_size>1_000_000:continue
    files.append(f)
for file in INCLUDE_FILES:
  f=ROOT/file
  if not f.is_file() or f.is_symlink():raise FileNotFoundError(file)
  files.append(f)
files=sorted(set(files),key=lambda f:f.relative_to(ROOT).as_posix())
rows=[]
for f in files:
  path=f.relative_to(ROOT).as_posix();data=f.read_bytes()
  if path.lower().endswith(REJECT_SUFFIXES):raise ValueError('forbidden proprietary/compiled extension '+path)
  if path.endswith('.zip') and path!='r96_result/AU57x_R96_SD_REVIEW_NOT_INSTALL.zip':
    raise ValueError('unauthorized nested archive')
  rows.append({'path':path,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
manifest={
 'version':'R96','baseline':'MHI2_CN_AU57x_K1006_2 / MU1005',
 'scope':'OFFLINE_HOST_LAB_AND_DOC_ONLY_CANDIDATE',
 'source_only':True,'installable':False,'safe_to_flash':False,'vehicle_verified':False,
 'genuine_mfi':False,'production_supervisor_bound':False,
 'includes_oem_binaries':False,'includes_apple_credentials':False,
 'step18_host_lab':'TESTED','step18_vehicle':'NOT_READY',
 'step19_document_review':'BUILT','step19_vehicle':'NOT_READY',
 'files':rows}
mp=ROOT/'RELEASE_MANIFEST_R96.json'
mp.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
sh=ROOT/'SHA256SUMS_R96.txt'
sh.write_text(''.join(f"{r['sha256']}  {r['path']}\n" for r in rows))
files=sorted(files+[mp,sh],key=lambda f:f.relative_to(ROOT).as_posix())
with zipfile.ZipFile(DST,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for f in files:
  path=f.relative_to(ROOT).as_posix();info=zipfile.ZipInfo(path,date_time=(2026,11,23,0,0,0))
  info.compress_type=zipfile.ZIP_DEFLATED
  info.create_system=3;info.external_attr=0o100644<<16
  z.writestr(info,f.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
with zipfile.ZipFile(DST) as z:
  assert z.testzip() is None
  assert len(z.namelist())==len(set(z.namelist()))==len(files)
  for r in rows:
    assert hashlib.sha256(z.read(r['path'])).hexdigest()==r['sha256']
print(json.dumps({'archive':str(DST),'entries':len(files),'bytes':DST.stat().st_size,
                 'sha256':hashlib.sha256(DST.read_bytes()).hexdigest(),
                 'source_only':True,'safe_to_flash':False,'vehicle_verified':False}))
