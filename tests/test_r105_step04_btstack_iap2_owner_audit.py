#!/usr/bin/env python3
"""Step 04 static-evidence gate. OEM binaries stay private; no QNX runtime."""
import json
from pathlib import Path
D=json.loads((Path(__file__).resolve().parents[1]/"docs/R105_STEP04_BTSTACK_IAP2_OWNER_STATIC_AUDIT.json").read_text())

def test_original_oem_elf_identities_are_pinned():
    assert D["inputs"]["bluetooth"]["sha256"]=="7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272"
    assert D["inputs"]["btstack"]["sha256"]=="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
    assert D["inputs"]["iapproxy"]["sha256"]=="44883ed28e44a8f06b5308266510981e17b3ee59d737f73208cd76e11ffc1c80"

def test_bluetooth_iap_markers_are_control_plane_only():
    b=D["inputs"]["bluetooth"]
    assert all(b["markers"][x]["present"] for x in ("bluetooth.enableIap","iapEnabled","SERVICETYPE_IAP2"))
    assert b["symbols"]["CBluetoothSmartphoneIntegration_vtable"]["size"]==36
    assert "not proof" in b["interpretation"]

def test_btstack_evidence_is_generic_rfcomm_sdp_not_iap2_owner():
    b=D["inputs"]["btstack"]
    assert b["symbols"]["BtJobStartService_vtable"]["size"]==56
    assert b["symbols"]["BtJobQuerySdp_vtable"]["size"]==56
    assert b["markers"]["RFCOMM_trace_channel"]["present"]
    assert b["markers"]["IapDevice_literal"] is False
    assert "does not expose an iAP2-specific" in b["interpretation"]

def test_iapproxy_is_not_misrepresented_as_byte_transport():
    p=D["inputs"]["iapproxy"]
    assert "asi.connectivity.bluetooth.iap.IapDeviceServices" in p["markers"]
    assert "not a Bluetooth RFCOMM byte-stream" in p["interpretation"]

def test_unproven_owner_edges_remain_closed():
    edges={x["edge"]:x["status"] for x in D["owner_chain"]}
    assert edges["bluetooth.enableIap -> iAP2 service creation in btstack"]=="NOT_PROVEN"
    assert edges["RFCOMM callback -> R45 ingress in actual btstack process"]=="NOT_RUNTIME_BOUND"
    assert edges["raw RFCOMM bytes -> mm-ipod transport callback -> iAP2 driver"]=="NOT_PROVEN"
    v=D["verdict"]
    assert v["mu1005_iap2_specific_rfcomm_owner"]=="NOT_PROVEN"
    assert v["installable"] is False and v["safe_to_flash"] is False

if __name__=="__main__":
    for name,fn in sorted(globals().copy().items()):
        if name.startswith("test_"):
            fn();print("PASS",name)
    print("R105 STEP04 BTSTACK IAP2 OWNER STATIC GATE PASS (OWNER NOT PROVEN / NO RUNTIME)")
