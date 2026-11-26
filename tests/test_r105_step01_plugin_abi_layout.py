#!/usr/bin/env python3
"""R105 Step 01: host-side structural assertions for the pinned ARM32 OEM ABI map.
This validates the documented layout only; it does not execute an OEM plugin.
"""
from pathlib import Path
import re, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[1]
HDR=ROOT/"project/source/src/cp_ipod_module_layout.h"
SRC=HDR.read_text()

def test_module_layout_contract_is_explicit():
    assert "STRUCTURAL VIEW" in SRC
    assert "not a callable ABI or plugin" in SRC
    assert "abi_version" in SRC and "0x200" in SRC
    assert "private_descriptor" in SRC

def test_seven_callback_slots_and_offsets_are_documented():
    expected={
        "init":"0x14","link_params_get":"0x18","type_get":"0x1c",
        "caps_get":"0x20","audiopath_get":"0x24",
        "data_send":"0x28","data_recv":"0x2c",
    }
    for field,offset in expected.items():
        assert f"uint32_t {field};" in SRC, field
        assert f"/* +{offset}" in SRC, offset

def test_arm32_layout_static_asserts_compile():
    source=r'''
#include <stddef.h>
#include "project/source/src/cp_ipod_module_layout.h"
_Static_assert(sizeof(struct cp_ipod_callback_prefix)==48,"callback prefix");
_Static_assert(sizeof(struct cp_ipod_module_layout)==36,"module layout");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,init)==0x14,"init");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,link_params_get)==0x18,"link");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,type_get)==0x1c,"type");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,caps_get)==0x20,"caps");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,audiopath_get)==0x24,"audio");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,data_send)==0x28,"send");
_Static_assert(offsetof(struct cp_ipod_callback_prefix,data_recv)==0x2c,"recv");
_Static_assert(offsetof(struct cp_ipod_module_layout,private_descriptor)==0x20,"private");
int main(void){return 0;}
'''
    with tempfile.TemporaryDirectory() as d:
        c=Path(d)/"abi.c"; exe=Path(d)/"abi"
        c.write_text(source)
        subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                        "-I",str(ROOT),str(c),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)

def test_unresolved_contract_is_not_promoted_to_fact():
    audit=(ROOT/"docs/STEP05_MU1005_USB_PLUGIN_ABI_AUDIT.json").read_text()
    assert '"proven_wireless_plugin": false' in audit
    assert '"rfcomm_actual_owner_verified": false' in audit
    assert '"vehicle_installable": false' in audit

if __name__=="__main__":
    for name,fn in sorted(globals().copy().items()):
        if name.startswith("test_"):
            fn();print("PASS",name)
    print("R105 STEP01 OEM PLUGIN ABI LAYOUT GATE PASS (HOST STRUCTURE ONLY)")
