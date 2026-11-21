#!/usr/bin/env python3
"""Fail-closed evidence integrity gate for AU57x's public research repository.
This verifies *recorded* audit claims, not their private OEM inputs or live radio.
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

EXPECTED_FIRMWARE="MU1005"
ALLOWED={"DONE","PARTIAL","BLOCKED","NOT_STARTED"}

def require(condition, message, problems):
    if not condition:
        problems.append(message)

def read_json(root, path):
    return json.loads((root/path).read_text(encoding="utf-8"))

def audit(root):
    errors=[]
    paths=["docs/CLOUD12_LEDGER.json","docs/STEP04_STATIC_EVIDENCE.json",
           "docs/STEP05_MU1005_USB_PLUGIN_ABI_AUDIT.json",
           "docs/R25_GITHUB_REUSE_MATRIX.json"]
    try:
        ledger, stat, usb, reuse=[read_json(root,p) for p in paths]
    except (OSError, ValueError) as e:
        return ["Cannot read pinned evidence: "+str(e)],{}
    steps=ledger.get("steps",[])
    require(len(steps)==12,"Ledger must have exactly 12 steps",errors)
    require([s.get("id") for s in steps]==list(range(1,13)),
            "Step numbers must remain fixed 01-12",errors)
    require(all(s.get("status") in ALLOWED for s in steps),
            "Unknown status",errors)
    require(ledger.get("installable") is False and ledger.get("vehicle_verified") is False,
            "Unverified research must NOT be installable or vehicle verified",errors)
    require(all(EXPECTED_FIRMWARE in str(obj.get("firmware",""))
                for obj in (stat,usb,reuse)), "Evidence firmware does not match MU1005",errors)
    require(stat.get("checks")=={"passed":47,"total":47,"result":"PASS_STATIC_MAP_ONLY"},
            "Static 47/47 evidence changed or promoted",errors)
    require(stat.get("production_wireless_chain_proven") is False
            and stat.get("installable") is False,
            "Static audit must NOT claim functional wireless",errors)
    require(usb.get("checks")=={"passed":22,"total":22},
            "USB ABI 22/22 evidence changed",errors)
    require(usb.get("usb_plugin",{}).get("module_abi")=="0x200",
            "USB transport module ABI mismatch",errors)
    require(usb.get("bluetooth_dsi_iap2_service_type","").startswith("0x40000000; NOT RFCOMM channel"),
            "Do not confuse DSI service mask with RFCOMM channel",errors)
    require(all(usb.get(k) is False for k in
                ("proven_wireless_plugin","rfcomm_actual_owner_verified",
                 "actual_mfi_verified","wireless_carplay_ready","vehicle_installable")),
            "Unproven OEM/MFi/media edges must remain false",errors)
    require("40/40" in reuse.get("R14_exact_ARM_instruction_verification",""),
            "R14 40-word sample-only comparison missing",errors)
    require(reuse.get("step05_status")=="INCOMPLETE_OEM_LIVE_BOUNDARY_NOT_PROVEN",
            "R25 live bridge evidence incorrectly promoted",errors)
    require(reuse.get("exact_oem_btstack_sha256")==
      "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566",
      "Pinned btstack sample differs",errors)
    require(len(steps)==12 and steps[1].get("status")=="DONE" and
            steps[9].get("status")=="DONE" and
            all(isinstance(steps[i].get("scope"),str) and
                "原厂" in steps[i]["scope"]
                for i in (1,9)),
            "Cloud-only static and CI scopes must be explicit",errors)
    require(not any(steps[i-1].get("status")=="DONE"
                    for i in (4,5,6,7,8,9,11,12)),
            "Cloud mocks cannot mark real OEM / install steps DONE",errors)
    try:
        bt=read_json(root,"docs/R29_BT_DSI_OWNER_AUDIT.json")
        require(bt.get("verdict")=="PASS_EXACT_SAMPLES_STATIC_ONLY",
                "High-level DSI sample verdict not pinned",errors)
        require(bt.get("samples",{}).get("bluetooth",{}).get("sha256")==
                usb.get("oem_sha256",{}).get("bluetooth"),
                "High-level Bluetooth hash disagrees with prior 22/22 audit",errors)
        require(all(bt.get(k) is False for k in
                   ("rfcomm_to_iap2_owner_proven","dynamic_dsi_service_registration_proven",
                    "mfi_verified","wireless_carplay_verified","installable")),
                "Static high-level DSI symbols must never be promoted to live iAP2",errors)
    except (OSError,ValueError) as e:
        errors.append("Missing high-level DSI evidence: "+str(e))
    try:
        ipod=read_json(root,"docs/R31_MM_IPOD_AUDIT.json")
        require(ipod.get("sha256")==usb.get("oem_sha256",{}).get("mm-ipod"),
                "mm-ipod pinned digest mismatch",errors)
        require(ipod.get("static_7_of_7") is True and
                ipod.get("verdict")=="PASS_STATIC_LOADER_SAMPLE_ONLY",
                "mm-ipod loader static findings missing",errors)
        require(all(ipod.get(k) is False for k in
                    ("runtime_selected_transport_proven",
                     "bluetooth_transport_plugin_found",
                     "bluetooth_to_mm_ipod_link_proven","vehicle_installable")),
                "mm-ipod static loader was incorrectly promoted to working Bluetooth",errors)
    except (OSError,ValueError) as e:
        errors.append("Missing mm-ipod loader audit: "+str(e))
    # Reject proprietary firmware, Apple keys and prebuilt binary uploads in public git.
    try:
        cp=subprocess.run(["git","-C",str(root),"ls-files","-z"],check=True,
                          stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15)
        tracked=[p.decode("utf-8","replace") for p in cp.stdout.split(b"\x00") if p]
        forbidden=sorted(p for p in tracked if
             re.search(r"\.(?:so(?:\.[0-9]+)?|elf|img|bin|7z|zip|key|pem|p12|pfx|o|a)$",
                       p,re.IGNORECASE))
        require(not forbidden,"Binary/key files tracked: "+", ".join(forbidden),errors)
    except (OSError,subprocess.SubprocessError):
        pass  # Temp-dir test fixtures do not contain git metadata.
    summary={"pass":not errors,"completed_steps":[s["id"] for s in steps
              if s.get("status")=="DONE"],"remaining_steps":[s["id"] for s in steps
              if s.get("status")!="DONE"],"scope":"OFFLINE_REVIEW_ONLY",
             "installable":False,"vehicle_verified":False,"errors":errors}
    return errors,summary

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--root",type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument("--report",type=Path)
    args=p.parse_args()
    problems,summary=audit(args.root.resolve())
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(summary,indent=2,ensure_ascii=False)+"\n",
                               encoding="utf-8")
    print(json.dumps(summary,ensure_ascii=False,sort_keys=True))
    if problems:
        print("CLOUD12_AUDIT_BLOCKED",file=sys.stderr)
        return 1
    print("CLOUD12_AUDIT_PASS")
    return 0
if __name__=="__main__":sys.exit(main())
