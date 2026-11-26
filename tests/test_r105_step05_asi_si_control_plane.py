#!/usr/bin/env python3
"""R105 Step 05: contract/static evidence only; never an OEM runtime test."""
import json
from pathlib import Path
D=json.loads((Path(__file__).resolve().parents[1]/"docs/R105_STEP05_ASI_SI_CONTROL_PLANE_AUDIT.json").read_text())

def test_asi_contract_has_exact_nine_method_ids():
    m=D["asi_iap_device_services"]["methods"]
    assert [x["id"] for x in m]==list(range(9))
    assert len(m)==9
    assert m[3]["name"]=="stringRequest" and m[3]["reply"] is False
    assert m[4]["name"]=="stringResponse" and m[4]["reply"] is False

def test_asi_contract_contains_no_raw_frame_or_stream_method():
    m=D["asi_iap_device_services"]["methods"]
    names=" ".join(x["name"]+" "+" ".join(x["args"]) for x in m).lower()
    assert not any(x in names for x in ("byte[]","rawframe","stream","packet","payload"))
    assert D["asi_iap_device_services"]["server_search"]["interface_implementation_found"] is False
    assert D["asi_iap_device_services"]["server_search"]["app_img_elf_count"]==778

def test_smartphone_integration_contracts_are_control_plane():
    s=D["smartphone_integration_contracts"]
    assert s["BluetoothSmartphoneIntegration"]["method_count"]==9
    assert s["dsi.smartphoneintegration"]["method_count"]==46
    assert "CONTROL_PLANE" in s["BluetoothSmartphoneIntegration"]["classification"]
    assert "UI/input/metadata" in s["dsi.smartphoneintegration"]["classification"]

def test_libirc_identity_is_full_sha_and_not_transport_claim():
    x=D["libirc_adapter"]
    assert x["size_bytes"]==296740
    assert len(x["sha256"])==64
    assert x["sha256"]=="3277e0c410ef3be3ea8d752ccd142c706e156e7a621f69ea95e6dd174b2ba9b2"
    assert x["defines_smartphone_integrator_missing_irc_symbols"] is True
    assert "does not create" in x["interpretation"]

def test_raw_asi_bridge_closed_and_data_plane_redirected():
    d=D["decision"]
    assert d["ASI_IapDeviceServices_raw_iap2_transport"]=="CLOSED_NO_PROVIDER_NO_RAW_METHOD"
    assert d["continue_investment_in_ASI_raw_byte_bridge"] is False
    assert "dio_manager" in d["next_data_plane_target"]
    assert d["MFi_bypass"] is False
    assert d["installable"] is False and d["safe_to_flash"] is False

if __name__=="__main__":
    for name,fn in sorted(globals().copy().items()):
        if name.startswith("test_"):
            fn();print("PASS",name)
    print("R105 STEP05 ASI/SI CONTROL-PLANE GATE PASS (RAW ASI BRIDGE CLOSED / NO RUNTIME)")
