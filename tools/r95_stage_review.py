#!/usr/bin/env python3
"""R95: independent offline evidence matrix for steps 3-17.

Runs every available host contract without stopping on a failed/blocked step.
Never treats a fake OEM owner, fake MFi signature, localhost TCP or QEMU as
proof of an in-vehicle connection or completed OEM integration.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
# Reuse each test across rows; no duplicated smoke work.
TESTS={
 'launcher': 'ci/host_r94_launcher.sh',
 'loader': 'ci/host_bt_preload_probe.sh',
 'rf_register': 'ci/host_rf_arg_experiment.sh',
 'rf_io': 'ci/host_wcp_r45_ingress.sh',
 'bt_epoch': 'ci/host_wcp_autoconnect.sh',
 'iap2': 'ci/host_wcp_r45_remote.sh',
 'mfi': 'ci/host_wcp_r45_remote_mfi.sh',
 'handoff': 'ci/host_step04_session.sh',
 'radio': 'ci/host_radio_policy.sh',
 'ap': 'ci/host_cloud_pipeline.sh',
 'media': 'ci/host_cloud_media.sh',
 'wired': 'ci/host_handoff.sh',
 'sd': 'ci/host_sd_txn.sh',
 'r95_sd': 'python3 -m unittest discover -s tests -p test_r95_sd_plan_codegen.py -v',
 'r95_retry': 'python3 -m unittest discover -s tests -p test_r95_toolchain_retry.py -v',
}
STEPS={
3:('OEM supervisor startup hook', ('launcher','loader'), 'exact OEM config schema and supervisor runtime environment unverified'),
4:('Original BlueSDK callback thread',('loader',), 'actual btstack callback-thread owner is not bound'),
5:('RFCOMM registration',('rf_register',),'iAP2 profile argument is experimental, live registration unverified'),
6:('RFCOMM byte transport',('rf_io','iap2'),'physical BlueSDK RF events are unverified'),
7:('Phone/session generation',('bt_epoch','iap2'),'paired iPhone MAC authority unverified'),
8:('iAP2 transport handshake',('iap2',),'real handset iAP2 handshake is unverified'),
9:('OEM Apple MFi',('mfi',),'only fixture MFi certificate/signature tested; real OEM MFi unverified'),
10:('BT-to-Wi-Fi handoff',('handoff','ap'),'real phone-car handoff unverified'),
11:('2.4 GHz AP/network',('radio','ap'),'2.4 GHz stock hotspot and association unverified'),
12:('Bonjour discovery',('ap',),'real original Bonjour service advertisement unverified'),
13:('AirPlay receiver session',('ap',),'real original AirPlay DIO owner/session unverified'),
14:('Video/audio streams',('media',),'live iPhone media channels unverified'),
15:('SI/DIO/HMI wired precedence',('wired','ap'),'real MMI control event dispatch unverified'),
16:('Full wireless integration',('iap2','mfi','ap','media'),'mock end-to-end only; no real device-to-vehicle connection'),
17:('SD backup and rollback',('sd','r95_sd'),'host/QNX transaction model verified, but stock image mount, actual config and boot health callback unverified'),
}

def run(out):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    results={}
    for k,cmd in TESTS.items():
        print(f'R95 TEST {k} START',flush=True)
        start=time.monotonic()
        try:
            x=subprocess.run(['bash',cmd] if cmd.startswith('ci/') else ['bash','-lc',cmd],cwd=ROOT,text=True,
                      stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
            status='PASS' if x.returncode==0 else 'FAIL'
            detail=f'exit={x.returncode}'
            log=x.stdout
        except subprocess.TimeoutExpired as exc:
            status='BLOCKED_TIMEOUT';detail='timeout after 180s';log=(exc.stdout or b'')
            if isinstance(log,bytes):log=log.decode('utf-8','replace')
        result={'result':status,'detail':detail,'elapsed_s':round(time.monotonic()-start,2),
                'log_file':f'{k}.log'}
        (out/f'{k}.log').write_text(log,encoding='utf-8')
        results[k]=result
        print(f'R95 TEST {k}: {status} ({result["elapsed_s"]}s)',flush=True)
    rows=[]
    for n,(title,keys,blocker) in STEPS.items():
        all_pass=all(results[k]['result']=='PASS' for k in keys)
        rows.append({'step':n,'title':title,'offline_test': 'PASS' if all_pass else 'BLOCKED',
                     'vehicle_complete':False,
                     'assessment':'OFFLINE_CONTRACT_ONLY' if all_pass else 'SKIPPED_TEST_FAILURE',
                     'test_keys':list(keys),'requires':blocker})
    report={'version':'R95','baseline':'MHI2_CN_AU57x_K1006_2 / MU1005',
            'generated_utc':datetime.now(timezone.utc).isoformat(),
            'vehicle_verified':False,'installable':False,'safe_to_flash':False,
            'scope':'OFFLINE_STEPS_03_TO_17_CONTINUOUS_NO_INSTALL',
            'tests':results,'steps':rows,
            'message':'Automated host contracts cannot prove OEM callback binding or genuine iPhone / Wi-Fi / AirPlay / MFi.'}
    (out/'R95_STEPS_03_TO_17_EVIDENCE.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    print('R95 MATRIX: %d/15 offline PASS, %d skipped/failed' %
          (sum(x['offline_test']=='PASS' for x in rows),sum(x['offline_test']!='PASS' for x in rows)),flush=True)
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',default='r95_result')
    a=p.parse_args()
    r=run(ROOT/a.out)
    return 0 if all(x['result']=='PASS' for x in r['tests'].values()) else 1
if __name__=='__main__':sys.exit(main())
