#!/usr/bin/env python3
"""Continue past individual failures. TEST COMPLETION ≠ genuine OEM owner binding."""
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'r97_result'; OUT.mkdir(exist_ok=True)
SUITES=[
 ('oem_got_relocation_mutations',['python3','-m','unittest','discover','-s','tests','-p','test_r97_stock_hfp_got_audit.py','-v']),
 ('actual_host_pthread_origin',['bash','ci/host_r97_owner_thread.sh']),
 ('hfp_got_callback_forwarding',['bash','ci/host_r97_hfp_got.sh']),
 ('preload_sha_and_process_identity',['bash','ci/host_bt_preload_probe.sh']),
 ('launcher_opt_in_rollback',['python3','-m','unittest','discover','-s','tests','-p','test_r97_hfp_launcher.py','-v']),
 ('original_launcher_staging',['bash','ci/host_r94_launcher.sh']),
 ('strict_c_42',['bash','ci/host_r95_source_compile.sh']),
 ('r96_install_rollback_review',['bash','ci/host_r96_to_step19.sh']),
 ('stock_r45_tcp_iap2',['bash','ci/host_wcp_r45_remote_mfi.sh']),
]

def run():
    results={}
    for name,cmd in SUITES:
        start=time.monotonic()
        try:
            done=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                                text=True,timeout=180)
            rc=done.returncode;output=done.stdout
        except subprocess.TimeoutExpired as e:
            rc=124
            output=e.stdout or b''
            if isinstance(output,bytes):output=output.decode('utf-8','replace')
        (OUT/(name+'.log')).write_text(output,encoding='utf-8')
        results[name]={'result':'PASS' if rc==0 else 'FAIL',
                       'exit':rc,'seconds':round(time.monotonic()-start,2),
                       'log':name+'.log'}
        print('R97 CONTINUE',name,results[name]['result'],flush=True)
    report={
        'version':'R97','baseline':'MHI2_CN_AU57x_K1006_2/MU1005',
        'genuine_oem_binary_static_audit':'PRIVATE_LOCAL_ONLY_NOT_IN_CI',
        'r97_owner_thread':'HOST_CODE_TESTED_NOT_OEM_THREAD_VERIFIED',
        'r97_hfp_got':'HOST_FORWARDING_TESTED_NOT_OEM_CALLBACK_OBSERVED',
        'oem_hfp_got_opt_in':'OFF_BY_DEFAULT',
        'iap2_native_registration':'MISSING',
        'apple_mfi_actual_handset':'MISSING',
        'airplay_actual_vehicle':'MISSING',
        'stage18_19_vehicle_installer':'NOT_READY',
        'installable':False,'vehicle_verified':False,'safe_to_flash':False,
        'tests':results,'passed':sum(r['result']=='PASS' for r in results.values()),
        'total':len(results),
        'warning':'A test is NOT an original QNX runtime or Bluetooth RF/iAP2 handshake.'}
    (OUT/'R97_NATIVE_OWNER_AND_HFP_EVIDENCE.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    print('R97 TOTAL HOST SUITES',report['passed'],'/',report['total'],flush=True)
    return report

if __name__=='__main__':
    info=run();sys.exit(0 if info['passed']==info['total'] else 1)
