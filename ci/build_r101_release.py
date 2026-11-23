#!/usr/bin/env python3
"""Build deterministic R101 NO_INSTALL source-only release with SHA256 manifest."""
import hashlib
import json
import pathlib
import zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
BASE=pathlib.Path('/mnt/data/AU57x_R100_Factory_to_Graft_FailClosed_Source_ONLY_NO_INSTALL.zip')
OUT=pathlib.Path('/mnt/data/AU57x_R101_RFCOMM_SCN_OEM_Dataflow_FIX1_Source_ONLY_NO_INSTALL.zip')
SOURCE_FILES=[
 'README.md','R101_BUILD_INFO.txt','DO_NOT_INSTALL_R101.txt',
 'docs/R101_RFCOMM_SERVER_CHANNEL_CN.md',
 'project/source/legacy_mu1005/bluetooth/inbound.c',
 'project/source/legacy_mu1005/bluetooth/inbound.h',
 'project/source/legacy_mu1005/bluetooth/rf_arg_experiment.c',
 'project/source/legacy_mu1005/bluetooth/rf_arg_experiment.h',
 'project/source/legacy_mu1005/bluetooth/native_binding.c',
 'project/source/legacy_mu1005/bluetooth/native_binding.h',
 'project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.c',
 'tests/test_r101_scn_contract.c','tests/test_r101_rfcomm_scn_audit.py',
 'tests/test_rf_arg_experiment.c','tests/test_r98_iap2_owner_handoff.c',
 'tests/test_mu1005_rf_register_sites.py',
 'tools/r101_rfcomm_scn_audit.py',
 'ci/host_r101_scn_audit.sh','ci/host_r101_continuous.py','ci/build_r101_release.py',
 '.github/workflows/cloud12-review.yml','.github/workflows/qnx65-compile.yml',
 'r101_result/R101_MU1005_SCN_AUDIT.json','r101_result/R101_EVIDENCE.json',
 'r101_result/r100_12_suite_inherited.log',
 'r101_result/r101_original_rfcomm_scn_range_and_snapshot.log',
 'r100_result/R100_CONTINUOUS_RESULTS.json'
]

def build():
 if OUT.exists():raise ValueError('R101 release already exists (refuse silent overwrite)')
 with zipfile.ZipFile(BASE) as z:
  data={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
 for n in SOURCE_FILES:
  p=ROOT/n
  if not p.is_file() or p.is_symlink():raise ValueError('Missing/symlink '+n)
  data[n]=p.read_bytes()
 for n in list(data):
  if n.startswith('RELEASE_MANIFEST_') or n.startswith('SHA256SUMS_'):
   del data[n]
 for n in data:
  if n.startswith('/') or '..' in pathlib.PurePosixPath(n).parts:
   raise ValueError('Unsafe archive path '+n)
  if any(k in n.lower() for k in ('private_oem','/btstack.elf','/bluetooth.elf','original_bluetooth.elf',
          'ipod-drvr-iap2.so','connectivity_launcher','mfi_private')):
   raise ValueError('Proprietary image filename '+n)
  if n.lower().endswith(('.elf','.so','.7z','.img','.bin','.key','.pem','.p12','.pfx','.spk','.zip')):
   raise ValueError('Non-source embedded content '+n)
 audit=json.loads(data['r101_result/R101_MU1005_SCN_AUDIT.json'])
 result=json.loads(data['r101_result/R101_EVIDENCE.json'])
 if audit['sha256']!='51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566':
  raise ValueError('Wrong pinned original hash')
 if audit['instruction_sites_checked']!=13 or audit['iap2_scn_discovered']:
  raise ValueError('Wrong OEM proof level')
 if result['aggregate_suites_passed']!=13 or result['installable'] or result['vehicle_verified']:
  raise ValueError('Incomplete software verification')
 manifest={
  'release':'R101','revision':'FIX1','baseline':'MHI2_CN_AU57x_K1006_2 / MU1005',
  'source_only':True,'installable':False,'safe_to_flash':False,'vehicle_verified':False,
  'native_rfcomm_owner_proven':False,'original_iap2_scn_proven':False,
  'host_test_suite_count':13,'native_module_compile_count':44,
  'changes':['OEM precise 13-instruction RF arg3 dataflow',
             'SCN 1..30 protocol descriptor publication vs 1..6 old cap',
             'reject invalid verified arg3 0/31/255',
             'session-frozen arg3 into R45 native binding'],
  'files':{n:hashlib.sha256(content).hexdigest() for n,content in sorted(data.items())}}
 data['RELEASE_MANIFEST_R101.json']=(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n').encode()
 data['SHA256SUMS_R101.txt']=''.join(f'{hashlib.sha256(b).hexdigest()}  {n}\n' for n,b in sorted(data.items())).encode()
 with zipfile.ZipFile(OUT,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for n,b in sorted(data.items()):
   i=zipfile.ZipInfo(n,date_time=(2026,11,23,0,0,0))
   i.compress_type=zipfile.ZIP_DEFLATED
   i.external_attr=((0o100755 if n.endswith('.sh') else 0o100644)<<16)
   z.writestr(i,b)
 with zipfile.ZipFile(OUT) as z:
  assert z.testzip() is None
  assert len(z.namelist())==len(set(z.namelist()))
  m=json.loads(z.read('RELEASE_MANIFEST_R101.json'))
  for n,d in m['files'].items():
   if hashlib.sha256(z.read(n)).hexdigest()!=d:raise ValueError('Archive member mismatch '+n)
 print(json.dumps({'archive':str(OUT),'sha256':hashlib.sha256(OUT.read_bytes()).hexdigest(),
                   'size_bytes':OUT.stat().st_size,'files':len(data),
                   'installable':False,'vehicle_verified':False},indent=2))
if __name__=='__main__':build()
