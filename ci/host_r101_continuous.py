#!/usr/bin/env python3
"""R101 all 13 offline suites; never treats source compilation as a vehicle run."""
import json
from pathlib import Path
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'r101_result';OUT.mkdir(exist_ok=True)
suites=[('r100_12_suite_inherited',['python3','ci/host_r100_continuous.py']),
        ('r101_original_rfcomm_scn_range_and_snapshot',['bash','ci/host_r101_scn_audit.sh'])]
results=[]
for name,cmd in suites:
    start=time.monotonic()
    try:
        r=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=360)
        rc=r.returncode;log=r.stdout+r.stderr
    except subprocess.TimeoutExpired as e:
        rc=124;log=str(e)
    (OUT/(name+'.log')).write_text(log)
    print('R101',name,'PASS' if rc==0 else 'FAIL',flush=True)
    results.append({'name':name,'exit':rc,'passed':rc==0,'seconds':round(time.monotonic()-start,2)})
old=ROOT/'r100_result/R100_CONTINUOUS_RESULTS.json'
inherited=json.loads(old.read_text()) if old.exists() else None
inherited_pass=bool(inherited and inherited.get('total')==12 and inherited.get('passed')==12)
result={
 'release':'R101','baseline':'MHI2_CN_AU57x_K1006_2 / MU1005',
 'change':'R14 original rfcomm slot range 1..30 and session-frozen OEM arg3',
 'host_legacy_suites_passed':12 if inherited_pass else 0,
 'new_offline_suite_passed':results[1]['passed'],
 'aggregate_suites_passed':13 if inherited_pass and all(x['passed'] for x in results) else None,
 'aggregate_suites_total':13,'strict_c_modules_passed':44 if inherited_pass else 0,
 'static_original_ELF_audited_locally':(OUT/'R101_MU1005_SCN_AUDIT.json').exists(),
 'oem_rf_api_arg3_sc_number_proven':False,
 'original_iap2_scn_proven':False,
 'in_car_hook_proven':False,
 'apple_mfi_verified':False,'wifi_airplay_live':False,
 'installable':False,'safe_to_flash':False,'vehicle_verified':False,
 'suites':results,
 'unresolved':['OEM iAP2 SDK event-thread and registration parameter not proven',
               'Original connectivity launcher and live SD rollback owner unavailable',
               'No original Apple iAP2/MFi/CarPlay session proof']}
(OUT/'R101_EVIDENCE.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
print('R101_TOTAL',result['aggregate_suites_passed'],'/',result['aggregate_suites_total'],flush=True)
sys.exit(0 if result['aggregate_suites_passed']==13 else 1)
