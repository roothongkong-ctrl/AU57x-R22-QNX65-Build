#!/usr/bin/env python3
"""Source-level fail-closed gate: no blocking connect from OEM iAP2 owner path."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.c").read_text()
HDR=(ROOT/"project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.h").read_text()

def body(name):
    m=re.search(r"\b(?:static\s+)?int\s+"+re.escape(name)+r"\s*\([^;]*?\)\s*\{",SRC,re.S)
    assert m, f"missing function {name}"
    start=m.end(); depth=1;i=start
    while i<len(SRC) and depth:
        if SRC[i]=="{":depth+=1
        elif SRC[i]=="}":depth-=1
        i+=1
    assert depth==0, f"unterminated {name}"
    return SRC[start:i-1]

def code_only(s):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", s, flags=re.S)

def test_legacy_native_bind_is_fail_closed():
    b=code_only(body("au98_bind_stock_native"))
    assert "return -1;" in b
    assert "au98_publish(" not in b
    assert "connect(" not in b and "poll(" not in b

def test_native_owner_path_only_adopts_worker_prepared_fd():
    b=code_only(body("au98_drive_stock_native"))
    assert "au98_drive(" in b
    assert "adopt_dispatch" in b
    assert "au98_publish(" not in b
    assert "au98_bind_stock_native(" not in b

def test_native_adopt_dispatch_never_creates_socket():
    b=code_only(body("adopt_dispatch"))
    assert "adopt_stock_common(" in b
    for name in ("adopt_dispatch","adopt_stock_common","au98_adopt","au98_adopt_with_ack","adopt_common"):
        code=code_only(body(name))
        assert not re.search(r"\b(?:socket|connect|connected|au98_prepare_daemon_socket|au98_publish)\s*\(",code),name
        # socket_ready uses only zero-timeout polling; the adoption call chain
        # must never perform its own waiting poll.
        assert not re.search(r"\bpoll\s*\(",code),name
    common=code_only(body("adopt_stock_common"))
    assert "au98_adopt_with_ack(" in common and "au98_adopt(" in common
    assert "mu_bt_native_attach_ack_context" in common
    ready=code_only(body("socket_ready"))
    assert re.search(r"\bpoll\s*\(\s*&p\s*,\s*1\s*,\s*0\s*\)",ready)
    assert "MSG_PEEK" in ready
    for name in ("au98_drive_stock_native","au98_drive_stock_native_ack"):
        code=code_only(body(name))
        assert "au98_drive(" in code and "adopt_dispatch" in code
        assert "au98_publish(" not in code and "au98_bind_stock_native(" not in code

def test_blocking_connect_is_worker_only_api():
    b=code_only(body("au98_prepare_daemon_socket"))
    assert "connected(port)" in b
    assert "socket(" not in b
    assert "Worker-side bounded connect" in HDR
    assert "Worker transfers exclusive prepared nonblocking fd" in HDR

def test_owner_callback_contract_is_explicit():
    assert "original serialized iAP2 owner, not HFP" in HDR
    assert "Wrong-thread calls are rejected" in HDR

if __name__=="__main__":
    for name,fn in sorted(globals().copy().items()):
        if name.startswith("test_"):
            fn();print("PASS",name)
    print("R105 OWNER NONBLOCKING SOURCE GATE PASS")
