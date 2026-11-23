#!/usr/bin/env python3
"""R100 continuous offline regression; failures do not abort later suites."""
import json
import pathlib
import subprocess
import sys
import time
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'r100_result'
OUT.mkdir(exist_ok=True)
SUITES=[
 ('strict_compile_44',['bash','ci/host_r95_source_compile.sh']),
 ('exact_oem_feature_detector_negative',['bash','ci/host_r100_route_audit.sh']),
 ('iap2_live_loopback_fixture',['bash','ci/host_r98_iap2_handoff.sh']),
 ('original_iap2_dual_config_23',['bash','ci/host_r99_stock_iap2.sh']),
 ('native_hfp_callback_observer',['bash','ci/host_r97_hfp_got.sh']),
 ('native_worker_owner_lifecycle',['bash','ci/host_r97_owner_thread.sh']),
 ('original_process_preload_identity',['bash','ci/host_bt_preload_probe.sh']),
 ('pinned_stock_launcher_config',['bash','ci/host_r94_launcher.sh']),
 ('c_sd_transaction_backup_recovery',['bash','ci/host_sd_txn.sh']),
 ('r96_supervised_recovery',['bash','ci/host_r96_oneclick.sh']),
 ('r45_iap2_mfi_gates_fixture',['bash','ci/host_wcp_r45_remote_mfi.sh']),
 ('wired_first_priority',['bash','ci/host_cloud_pipeline.sh']),
]
def main():
    results=[]
    for name,args in SUITES:
        start=time.monotonic()
        try:
            r=subprocess.run(args,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                             text=True,timeout=240)
            rc=r.returncode; out=r.stdout
        except subprocess.TimeoutExpired as e:
            rc=124;out=e.stdout or b''
            if isinstance(out,bytes):out=out.decode('utf8','replace')
        (OUT/(name+'.log')).write_text(out,encoding='utf8')
        item={'name':name,'result':'PASS' if rc==0 else 'FAIL',
              'exit':rc,'seconds':round(time.monotonic()-start,2)}
        results.append(item)
        print('R100',name,item['result'],flush=True)
    status={
     'release':'R100','baseline':'MHI2_CN_AU57x_K1006_2 / MU1005',
     'scope':'FACTORY_TO_GRAFT_FAIL_CLOSED_QNX_NATIVE_SOURCE',
     'authorized_route':'GRAFT_RESEARCH_OPT_IN_APPROVED_BY_USER',
     'factory_iap2_runtime_proven':False,'graft_blue_sdk_owner_runtime_proven':False,
     'oem_prepinned_ELF_static_audit':'PRIVATE_LOCAL_PASS',
     'live_iap2_mfi_apple_proof':False,'ready_to_install':False,'safe_to_flash':False,
     'wired_carplay_preserved_in_production': 'REQUIRED_NOT_VEHICLE_TESTED',
     'vehicle_verified':False,
     'suites':results,
     'passed':sum(x['result']=='PASS' for x in results),
     'total':len(results),
     'limitations':['exact OEM connectivity.json not available',
        'no original iAP2 BlueSDK owner callback or register parameter evidence',
        'real iPhone and MFi authorization not tested',
        'no QNX vehicle installer or boot recovery proof'],
    }
    (OUT/'R100_CONTINUOUS_RESULTS.json').write_text(json.dumps(status,indent=2,ensure_ascii=False)+'\n')
    print('R100_TOTAL',status['passed'],'/',status['total'],flush=True)
    return 0 if status['passed']==status['total'] else 1
if __name__=='__main__':sys.exit(main())
