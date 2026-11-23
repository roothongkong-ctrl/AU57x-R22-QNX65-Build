#!/usr/bin/env python3
"""Reproducible research ZIP; never produces a live Audi installer."""
from __future__ import annotations
import hashlib, json, os, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEST=Path('/mnt/data/AU57x_R103_LuKa_iAP2_Link_Bridge_Source_ONLY_NO_INSTALL.zip')
SKIP={'.git','.github_cache','__pycache__','target','.venv','_tmp'}
files=[]
for f in ROOT.rglob('*'):
    if not f.is_file() or f.is_symlink() or any(x in SKIP for x in f.relative_to(ROOT).parts) or f.suffix in ('.pyc','.zip'):
        continue
    if f.stat().st_size>2_000_000: continue
    files.append(f)
files.sort(key=lambda p:p.relative_to(ROOT).as_posix())
entries={f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
manifest={'version':'R103','description':'RESEARCH ONLY -- NO VEHICLE INSTALL','vehicle_verified':False,'safe_to_flash':False,'upstream_license':'GPL-3.0','files':entries}
with zipfile.ZipFile(DEST,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for f in files:
        rel=f.relative_to(ROOT).as_posix()
        info=zipfile.ZipInfo(rel,date_time=(2026,11,23,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
        z.writestr(info,f.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
    info=zipfile.ZipInfo('RELEASE_MANIFEST_R103.json',date_time=(2026,11,23,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
    z.writestr(info,json.dumps(manifest,ensure_ascii=False,sort_keys=True,indent=2).encode()+b'\n')
with zipfile.ZipFile(DEST) as z:
    bad=z.testzip();assert not bad,bad
    audit=json.loads(z.read('RELEASE_MANIFEST_R103.json'))
    assert all(hashlib.sha256(z.read(p)).hexdigest()==h for p,h in audit['files'].items())
    assert 'original' not in str(audit).lower() or audit['vehicle_verified'] is False
print(json.dumps({'zip':str(DEST),'size':DEST.stat().st_size,'files':len(files)+1,'sha256':hashlib.sha256(DEST.read_bytes()).hexdigest(),'zip_integrity':True,'safe_to_flash':False},ensure_ascii=False))
