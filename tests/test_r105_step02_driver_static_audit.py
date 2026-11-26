#!/usr/bin/env python3
"""Step 02 static audit evidence gate; metadata only, never a vehicle/runtime test."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
D=json.loads((ROOT/"docs/R105_STEP02_DRIVER_STATIC_AUDIT.json").read_text())

def test_exact_oem_driver_identity_is_pinned():
    assert D["size_bytes"]==739925
    assert D["sha256"]=="42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf"
    assert D["elf"]["class"]=="ELF32" and D["elf"]["machine"]=="ARM"

def test_transport_callback_dispatch_uses_expected_slots():
    t=D["transport_callback_dispatch"]
    assert t["transport_send_pkt"]["va"]=="0x2115c"
    assert t["transport_send_pkt"]["slot_offset"]=="0x28"
    assert t["transport_receive"]["slot_offset"]=="0x2c"
    assert t["transport_get_link_params"]["slot_offset"]=="0x18"
    assert "link_handle_iap2pkt" in t["transport_recv_pkt"]["uses"]

def test_bluetooth_feature_updates_are_not_mislabelled_as_rfcomm():
    b=D["bluetooth_iap2_feature_functions"]
    assert b["bt_start_updates"]["message_id"]=="0x4e03"
    assert b["bt_send_info"]["message_id"]=="0x4e01"
    assert "PPS state" in b["bt_update_recv"]["role"]
    assert D["limitations"]["RFCOMM_socket_or_BlueSDK_callback_inside_this_ELF"]=="not proven"

def test_wifi_configuration_message_is_not_claimed_as_radio_setup():
    w=D["wifi_iap2_feature"]
    assert w["wifi_acc_config_info"]["message_id"]=="0x5703"
    assert set(w["observed_parameters"])=={"SSID","Passphrase","SecurityType","Channel"}
    assert "not proof of radio/AP creation" in w["scope"]

def test_component_names_do_not_promote_unknown_selection_rules():
    names=D["component_metadata_strings"]
    assert "BluetoothTransportComponent" in names
    assert "WiFiTransportComponent" in names
    assert D["limitations"]["iap2_cfg_transport_selection_algorithm"]=="not proven"
    assert D["limitations"]["installable"] is False

if __name__=="__main__":
    for name,fn in sorted(globals().copy().items()):
        if name.startswith("test_"):
            fn(); print("PASS",name)
    print("R105 STEP02 MU1005 DRIVER STATIC AUDIT GATE PASS (NO RUNTIME)")
