#!/usr/bin/env python3
"""Read-only pinned MU1005 factory-vs-graft capability audit.

Only strings/headers/known dynamic symbol evidence. Absence of a string is NOT
proof that a C/C++ capability is missing. Never distribute OEM ELF inputs.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import sys

PINNED = {
    "btstack": "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566",
    "bluetooth": "7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272",
    "ipod-drvr-iap2.so": "42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf",
    "smartphone_integrator": "2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b",
    "connectivity_launcher": "08896149dcdcc2bc5a6b22ce78682ad5949fbeb8fdc0fb1249162861e40914b8",
}
CHECKS = {
    "btstack": {
        "builtin_rfcomm_text": b"RFCOMM",
        "hfp_callback_symbol": b"HfRfCallback",
        "sim_callback_symbol": b"SimClientRfCallback",
        "pthread_create_import_name": b"pthread_create",
        "iap_device_marker": b"/dev/iapDevice",
        "iap_services_marker": b"IapServices",
        "iap2_accessory_marker": b"BluetoothiAP2Link",
        "apple_iap_sdp_uuid_text": b"deca-fade",
    },
    "bluetooth": {
        "enable_iap_config_key": b"bluetooth.enableIap",
        "iap_enabled_key": b"iapEnabled",
        "iap2_service_enum": b"SERVICETYPE_IAP2",
    },
    "ipod-drvr-iap2.so": {
        "bluetooth_component_text": b"BluetoothTransportComponent",
        "bluetooth_iap2_link_text": b"BluetoothiAP2Link",
        "iap2_config_path": b"/etc/mm/iap2.cfg",
    },
    "smartphone_integrator": {
        "carplay_name": b"CarPlay",
    },
    "connectivity_launcher": {
        "stock_launcher_config_name": b"connectivity.json",
        "stock_btstack_module_name": b"btstack",
    },
}
# Executable images must be ET_EXEC, driver is ET_DYN. No QNX code runs.
TYPES = {"btstack": 2,"bluetooth": 2,"ipod-drvr-iap2.so": 3,
         "smartphone_integrator": 2,"connectivity_launcher": 2}


def check_bytes(name, data, pin=PINNED, checks=CHECKS, types=TYPES):
    expected = pin[name]
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected:
        raise ValueError(f"{name}: SHA256 mismatches original MU1005 image")
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4:7] != b"\x01\x01\x01":
        raise ValueError(f"{name}: not a verified little-endian ARM32 ELF")
    etype, machine = struct.unpack_from("<HH", data, 16)
    if etype != types[name] or machine != 40:
        raise ValueError(f"{name}: unexpected ELF type/machine")
    return {
        "sha256": digest, "size": len(data), "machine": "ARM32 little-endian QNX ELF",
        "type": "ET_DYN" if etype == 3 else "ET_EXEC",
        "markers": {k: {
            "found": (data.find(v) != -1),
            "offset_hex": (hex(data.find(v)) if data.find(v) != -1 else None),
            "evidence_scope": "LITERAL_MARKER_ONLY"
        } for k, v in checks[name].items()},
    }


def audit(paths):
    if set(paths) != set(PINNED):
        raise ValueError("exact five original MU1005 binary paths are required")
    result = {}
    for name, p in paths.items():
        path = pathlib.Path(p)
        if not path.is_file() or path.is_symlink() or path.stat().st_size > 10_000_000:
            raise ValueError(f"{name}: invalid/missing original private path")
        result[name] = check_bytes(name, path.read_bytes())
    # Concrete contrast: original OEM iAP2 driver contains BT descriptions;
    # original MU1005 btstack has NO corresponding plaintext IapServices names.
    b = result["btstack"]["markers"]
    d = result["ipod-drvr-iap2.so"]["markers"]
    state = {
        "kind": "R100_PINNED_ORIGINAL_MU1005_OFFLINE_ROUTE_AUDIT",
        "vehicle_verified": False, "installable": False, "safe_to_flash": False,
        "original_files_distributed": False,
        "factory_iap2_runtime_proven": False,
        "stock_btstack_named_iap_endpoint_observed": b["iap_device_marker"]["found"],
        "stock_btstack_named_iap_services_observed": b["iap_services_marker"]["found"],
        "iap2_driver_bluetooth_component_marker_observed": d["bluetooth_component_text"]["found"],
        "route_recommendation": "FALLBACK_GRAFT_RESEARCH" if (
            not b["iap_device_marker"]["found"] and not b["iap_services_marker"]["found"]
        ) else "FACTORY_SERVICE_RESEARCH",
        "route_eligible_for_auto_activation": False,
        "missing_graft_proofs": [
            "verified OEM supervisor launch config", "live original BlueSDK owner callback",
            "verified RFCOMM iAP2 registration profile", "trusted rollback controller",
            "live Apple original MFi path and AirPlay handoff"],
        "warning": "No matching plaintext string does not prove code or a feature is absent.",
        "original_binary_evidence": result,
    }
    return state


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-dir",required=True)
    p.add_argument("--out",required=True)
    x=p.parse_args()
    try:
        path=pathlib.Path(x.original_dir)
        result=audit({name:path/name for name in PINNED})
        target=pathlib.Path(x.out)
        if target.exists():raise ValueError("refuse to overwrite existing audit")
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(json.dumps(result,ensure_ascii=False,indent=2,sort_keys=True)+"\n")
        print(json.dumps({k:v for k,v in result.items() if k!="original_binary_evidence"},ensure_ascii=False))
    except (OSError,ValueError) as e:
        print("R100 DENY:",e,file=sys.stderr)
        return 2
    return 0
if __name__=="__main__":sys.exit(main())
