#!/usr/bin/env python3
"""R96 stage18-19 multi-suite final offline gate; never claims vehicle acceptance."""
import json
from datetime import datetime,timezone
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'r96_result'
SUITES=[
 ('strict_c_43','ci/host_r95_source_compile.sh'),
 ('native_sd_txn_67','ci/host_sd_txn.sh'),
 ('oneclick_supervised_12','ci/host_r96_oneclick.sh'),
 ('review_candidate_9','ci/host_r96_candidate.sh'),
 ('stock_blue_rfc','ci/host_wcp_r45_remote.sh'),
 ('stock_iap2_mfi_mock','ci/host_wcp_r45_remote_mfi.sh'),
 ('airplay_pipeline_fixture','ci/host_cloud_pipeline.sh'),
 ('wired_priority_fixture','ci/host_handoff.sh'),
]

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    results={}
    for name,cmd in SUITES:
        print('R96 SUITE',name,'START',flush=True)
        t=time.monotonic()
        try:
            p=subprocess.run(['bash',cmd],cwd=ROOT,text=True,stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,timeout=180)
            ok=(p.returncode==0)
            output=p.stdout
            reason=f'exit={p.returncode}'
        except subprocess.TimeoutExpired as e:
            ok=False;output=e.stdout or b''
            if isinstance(output,bytes):output=output.decode('utf-8','replace')
            reason='timed out'
        (OUT/(name+'.log')).write_text(output,encoding='utf-8')
        results[name]={'result':'PASS' if ok else 'FAIL','reason':reason,
                       'duration_s':round(time.monotonic()-t,2),
                       'log':name+'.log'}
        print('R96 SUITE',name,results[name]['result'],flush=True)
    review=OUT/'AU57x_R96_SD_REVIEW_NOT_INSTALL.zip'
    report={
      'version':'R96','baseline':'MHI2_CN_AU57x_K1006_2 / MU1005',
      'utc':datetime.now(timezone.utc).isoformat(),'previous_stage':'R95',
      'stage18_offline_auto_installer':'HOST_LAB_IMPLEMENTED' if results['oneclick_supervised_12']['result']=='PASS' else 'BLOCKED',
      'stage19_offline_source_review':'DOCUMENT_ONLY_BUILT' if results['review_candidate_9']['result']=='PASS' and review.is_file() else 'BLOCKED',
      'stage18_vehicle_installer':'NOT_READY','stage19_car_installable_candidate':'NOT_READY',
      'stage20':'NOT_ATTEMPTED',
      'can_flash':False,'vehicle_verified':False,'genuine_apple_mfi_verified':False,
      'stock_btstack_live_loaded':False,'production_airplay_verified':False,
      'tests':results,
      'offline_suites_passed':sum(x['result']=='PASS' for x in results.values()),
      'offline_suites_total':len(results),
      'unresolved_real_integration':['original MU1005 supervisor launch config and runtime loader',
         'real BlueSDK RFCOMM iAP2 callback ownership','genuine OEM MFi and iPhone link',
         'real 2.4GHz Wi-Fi AirPlay session','trusted startup health and rollback on head unit'],
      'note':'Every PASS is HOST/QNX OFFLINE only. Do not interpret as in-car installation.'}
    (OUT/'R96_STAGE18_19_EVIDENCE.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print('R96 OFFLINE SUITES %d/%d; CAR_INSTALLABLE=FALSE' % (report['offline_suites_passed'],report['offline_suites_total']))
    return report

if __name__=='__main__':
    sys.exit(0 if run()['offline_suites_passed']==len(SUITES) else 1)
