#!/usr/bin/env python3
"""R105 Step 06 metadata gate; private OEM ELF bytes are not in this repository."""
import json
from pathlib import Path
D=json.loads((Path(__file__).resolve().parents[1]/"docs/R105_STEP06_RESMGR_MOUNT_STATIC_AUDIT.json").read_text())
def test_exact_oem_inputs():
    assert D["inputs"]["mm_ipod"]["sha256"]=="4b87cd2d41ddcee2f38800fe8afe8b4b340e4f23e1d86c7e97af303522aa15d09"
    assert D["inputs"]["iap2_driver"]["sha256"]=="42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf"
def test_driver_mount_call_registers_and_arguments():
    c=D["driver_call"]; assert c["callsite"]=="0x63d0"; a=c["argument_setup"]
    assert a[0]["word"]=="0xe5909014" and a[0]["va"]=="0x5ff0"
    assert a[1]["word"]=="0xe1a00009" and a[1]["va"]=="0x63c8"
    assert a[2]["word"]=="0xe3a01001" and a[2]["va"]=="0x63cc"
    assert a[3]["word"]=="0xebfffa3e" and a[3]["va"]=="0x63d0"
def test_import_resolves_to_mm_ipod_export():
    d=D["inputs"]["iap2_driver"]["relocation"]; h=D["inputs"]["mm_ipod"]["exports"]["ipod_resmgr_mount"]
    assert d["symbol"]=="ipod_resmgr_mount" and d["plt_stub"]=="0x4cd0" and h["va"]=="0x103ad0"
def test_mode_zero_and_mode_one_are_not_conflated():
    h=D["host_mount_implementation"]
    assert h["mode_zero"]["path"]=="format('%s%d', '/dev/ipod', devno)"
    assert h["mode_nonzero"]["path_source"]=="*(context+0x04)"
    assert h["mode_nonzero"]["dispatch_source"]=="*(context+0x20)"
    assert h["attach"]["function"]=="resmgr_attach"
def test_unresolved_runtime_and_radio_gates_remain_closed():
    l=D["limitations"]
    assert l["custom_mode1_path_runtime_value"]=="unknown" and l["RFCOMM_to_driver_data_path"]=="not proven"
    assert D["verdict"]=="PASS_STATIC_MOUNT_OWNER_AND_CALLSITE_ONLY" and D["installable"] is False
if __name__=="__main__":
    for n,f in sorted(globals().copy().items()):
        if n.startswith("test_"): f(); print("PASS",n)
    print("R105 STEP06 STATIC MOUNT OWNER GATE PASS (NO RUNTIME)")
