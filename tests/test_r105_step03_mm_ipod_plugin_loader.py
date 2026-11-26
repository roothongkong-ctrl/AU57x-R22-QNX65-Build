#!/usr/bin/env python3
"""Step 03 host evidence gate: exact static facts, no QNX runtime execution."""
import json
from pathlib import Path
D=json.loads((Path(__file__).resolve().parents[1]/"docs/R105_STEP03_MM_IPOD_PLUGIN_LOADER_AUDIT.json").read_text())

def test_mm_ipod_loader_uses_dynamic_transport_module_name():
    e=D["mm_ipod_loader"]
    assert e["device_transport_init"]["va"]=="0x103510"
    assert e["transport_module_name_pattern"]=="ipod-transport-%s.so"
    assert e["driver_module_name_pattern"]=="ipod-drvr-%s.so"
    assert "resolves exported ipod_module with dlsym" in e["loader_evidence"]

def test_stock_usbdevice_plugin_callback_offsets_are_pinned():
    p=D["usbdevice_plugin"]
    assert p["ipod_module_symbol"]["size"]==36
    assert p["transport_context_symbol"]["size"]==168
    expected={"init":"0x14","get_link_params":"0x18","type_get":"0x1c","caps_get":"0x20","audiopath_get":"0x24","data_send":"0x28","data_recv":"0x2c"}
    for k,v in expected.items():
        assert p["context_slots"][k]["offset"]==v

def test_usbhost_donor_has_same_core_data_plane_offsets():
    p=D["usbhost_plugin"]["context_slots"]
    assert p["init"]["offset"]=="0x14"
    assert p["get_link_params"]["offset"]=="0x18"
    assert p["data_send"]["offset"]=="0x28"
    assert p["data_recv"]["offset"]=="0x2c"

def test_archive_inventory_does_not_invent_bt_or_wifi_plugin():
    a=D["archive_inventory"]
    assert a["ipod_transport_modules_found"]==["ipod-transport-usbdevice.so","ipod-transport-usbhost.so"]
    assert a["bluetooth_transport_module_found"] is False
    assert a["wifi_transport_module_found"] is False
    assert "does not prove absence" in a["scope"]

def test_bluetooth_iap_service_manager_is_not_claimed_as_raw_transport():
    b=D["stock_bluetooth_evidence"]["interpretation"]
    assert "do not establish" in b
    assert D["verdict"]["raw_rfcomm_to_iap2_driver_bridge"]=="NOT_PROVEN"
    assert D["verdict"]["installable"] is False

if __name__=="__main__":
    for name,fn in sorted(globals().copy().items()):
        if name.startswith("test_"):
            fn(); print("PASS",name)
    print("R105 STEP03 MM-IPOD PLUGIN LOADER STATIC GATE PASS (NO RUNTIME)")
