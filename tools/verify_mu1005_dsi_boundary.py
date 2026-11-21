#!/usr/bin/env python3
"""Read-only exact sample audit of the MU1005 Bluetooth DSI provider.
Runs only on locally held OEM ELFs. Never uploads or patches proprietary files.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

EXPECTED={
 "bluetooth":{
  "sha256":"7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272",
  "markers":[
   "_ZTVN10app_con_bt31CBluetoothSmartphoneIntegrationE",
   "_ZTVN3asi12connectivity9bluetooth49BluetoothSmartphoneIntegrationServiceRegistrationE",
   "_ZN10app_con_bt31CBluetoothSmartphoneIntegration11interfaceIDE",
   "iapEnabled","bluetooth.enableIap","SERVICETYPE_IAP2"
  ]
 },
 "btstack":{
  "sha256":"51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566",
  "markers":["__tracing__channel_CON_BTSTACK_IA_RFCOMM"]
 }
}

def verify(directory):
    result={"firmware":"MHI2_CN_AU57x_K1006_2/MU1005",
      "type":"EXACT_OEM_ELF_STATIC_SYMBOL_AUDIT",
      "samples":{},"source_only":True,"vehicle_firmware_compared":False,
      "rfcomm_to_iap2_owner_proven":False,
      "dynamic_dsi_service_registration_proven":False,
      "mfi_verified":False,"wireless_carplay_verified":False,
      "installable":False}
    passed=True
    for name,reference in EXPECTED.items():
        f=directory/name
        if not f.is_file():
            result["samples"][name]={"present":False,"sha256_match":False}
            passed=False
            continue
        b=f.read_bytes();digest=hashlib.sha256(b).hexdigest()
        markers={name:(name.encode("ascii") in b) for name in reference["markers"]}
        good=digest==reference["sha256"] and all(markers.values())
        result["samples"][name]={
            "present":True,"sha256":digest,"sha256_match":digest==reference["sha256"],
            "required_symbol_or_string_markers":markers,"sample_pass":good
        }
        passed=passed and good
    result["verdict"]="PASS_EXACT_SAMPLES_STATIC_ONLY" if passed else "BLOCKED_SAMPLE_MISMATCH"
    return result

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--oem-dir",required=True,type=Path)
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    r=verify(args.oem_dir)
    data=json.dumps(r,indent=2,ensure_ascii=False)+"\n"
    if args.output:args.output.write_text(data,encoding="utf-8")
    print(data)
    return 0 if r["verdict"]=="PASS_EXACT_SAMPLES_STATIC_ONLY" else 20

if __name__=="__main__":
    sys.exit(main())
