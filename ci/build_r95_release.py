#!/usr/bin/env python3
"""Reproducible source-only archive (never include private OEM donor binaries)."""
import hashlib
import json
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
DST=Path('/mnt/data/AU57x_R95_Steps03-17_Continuous_Offline_Source_ONLY_NO_INSTALL.zip')
INCLUDE_DIRS=('project','tools','ci','tests','docs','.github','R93_SD_MIB_ROOT')
INCLUDE_FILES=('README.md','LICENSE','NOTICE_THIRD_PARTY_CN.md','R95_BUILD_INFO.txt',
               'DO_NOT_INSTALL_R95.txt','r95_result/R95_STEPS_03_TO_17_EVIDENCE.json')
EXCLUDE=('__pycache__','.pyc','.so','.o','.elf','.img','.bin','.zip','.pem','.key','.p12','.pfx',
         'qnx_build_result','cloud12_result','r95_result','/out/','graft_au57x_candidate.c')
records=[]
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
 if not f.is_file():raise FileNotFoundError(file)
 files.append(f)
files=sorted(set(files),key=lambda f:f.relative_to(ROOT).as_posix())
for f in files:
 rel=f.relative_to(ROOT).as_posix();blob=f.read_bytes()
 records.append(dict(path=rel,size=len(blob),sha256=hashlib.sha256(blob).hexdigest()))
manifest=dict(version='R95',baseline='MHI2_CN_AU57x_K1006_2 / MU1005',
 source_only=True,installable=False,vehicle_verified=False,safe_to_flash=False,
 includes_oem_binaries=False,includes_apple_credentials=False,files=records)
mp=ROOT/'RELEASE_MANIFEST_R95.json'
mp.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
sh=ROOT/'SHA256SUMS_R95.txt'
sh.write_text(''.join(f"{row['sha256']}  {row['path']}\n" for row in records))
files += [mp,sh]
files=sorted(files,key=lambda f:f.relative_to(ROOT).as_posix())
with zipfile.ZipFile(DST,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for f in files:
  rel=f.relative_to(ROOT).as_posix()
  entry=zipfile.ZipInfo(rel,date_time=(2026,11,22,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED;entry.external_attr=(0o644<<16)
  z.writestr(entry,f.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
with zipfile.ZipFile(DST) as z:
 assert z.testzip() is None
 assert len(z.namelist())==len(set(z.namelist()))==len(files)
 for row in records:
  assert hashlib.sha256(z.read(row['path'])).hexdigest()==row['sha256']
print('R95 PACKAGE',DST,'files',len(files),'bytes',DST.stat().st_size,'SHA256',hashlib.sha256(DST.read_bytes()).hexdigest())
