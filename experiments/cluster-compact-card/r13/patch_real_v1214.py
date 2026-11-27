#!/usr/bin/env python3
"""Strict SOURCE-ONLY patch on exact MIB Cluster Map v1.2.14 protocol hook.

No patches to installed files. The compiled R13 experiment is inert by default.
Actual type112 /info advertisement needs BOTH:
 - compile flag CLUSTERMAP_R13_112_LAB
 - runtime CLUSTERMAP_R13_112_PROBE=1

The source adds a separate AES-decrypting type112 socket probe; it does NOT
deliver a real H264 renderer, and does not prove that GaoDe supplies type112.
"""
import sys
from pathlib import Path

src=Path(sys.argv[1] if len(sys.argv)>1 else
    "thirdparty/mhi2_altscreen_carplay/libaltscreen111_gen2.c")
s=src.read_text()
def change(old,new,label):
    global s
    n=s.count(old)
    if n!=1:
        raise SystemExit(f"FAIL CLOSED: {label}: expected exactly one anchor, got {n}")
    s=s.replace(old,new,1)

change(
    "CFDictionaryRef AirPlayCopyServerInfo(AirPlayReceiverSessionRef session,",
    """/* Experimental type112 is never compiled into normal MIB releases. */
#ifdef CLUSTERMAP_R13_112_LAB
static void call_stock_teardown(AirPlayReceiverSessionRef, CFDictionaryRef,
                                OSStatus, Boolean *);
#include "r13_112_probe.inc"
#define R13_TEARDOWN_CALL r13_stock_teardown_forward
#else
#define R13_TEARDOWN_CALL call_stock_teardown
#endif

CFDictionaryRef AirPlayCopyServerInfo(AirPlayReceiverSessionRef session,""",
    "include real hook probe before server info")

change(
    """        p_CFArrayAppendValue(displays,alt);
        p_CFDictionarySetValue(info,kdisplays,displays);""",
    """        p_CFArrayAppendValue(displays,alt);
#ifdef CLUSTERMAP_R13_112_LAB
        /* Only opt-in: 111 remains untouched; 112 gets independent UUID. */
        r13_advertise(displays,alt);
#endif
        p_CFDictionarySetValue(info,kdisplays,displays);""",
    "real /info additional card display")

change(
    """    e=call_stock_setup(s,request,&stockResp);
    logf_u2("SETUP stock result=%d response=%s", """,
    """#ifdef CLUSTERMAP_R13_112_LAB
    /* Stock does not own the optional type112; hide it from stock SETUP.
     * Do not alter the ordinary type110+111 request otherwise. */
    CFMutableDictionaryRef r13_stock_request=NULL;
    if(r13_enabled() && r13_find112(request)){
        r13_stock_request=r13_strip112(request);
        if(!r13_stock_request){
            logf_u2("R13 cannot strip 112 from stock SETUP: preserve stock path");
        }
    }
    e=call_stock_setup(s,r13_stock_request?r13_stock_request:request,&stockResp);
    if(r13_stock_request)p_CFRelease(r13_stock_request);
#else
    e=call_stock_setup(s,request,&stockResp);
#endif
    logf_u2("SETUP stock result=%d response=%s", """,
    "stock SETUP 112 isolation")

change(
    """    if(stockResp)set_reference_enabled_features((CFMutableDictionaryRef)stockResp);

    hasAlt=""",
    """    if(stockResp)set_reference_enabled_features((CFMutableDictionaryRef)stockResp);
#ifdef CLUSTERMAP_R13_112_LAB
    /* Independent REAL 112 TCP socket + per-connection stock AES derivation.
     * Receiver only logs bounded packets, never renders user navigation. */
    if(stockResp && r13_enabled())
        r13_setup(s,request,(CFMutableDictionaryRef)stockResp);
#endif

    hasAlt=""",
    "append real type112 SETUP descriptor")

change(
    """    if(!g_enabled){
        call_stock_teardown(s,request,reason,outDone);
        return;
    }

    has_alt =""",
    """#ifdef CLUSTERMAP_R13_112_LAB
    if(!request || r13_find112(request))r13_stop_receiver();
#endif
    if(!g_enabled){
        R13_TEARDOWN_CALL(s,request,reason,outDone);
        return;
    }

    has_alt =""",
    "teardown independent 112 first")

change(
    """            call_stock_teardown(s,f?f:request,reason,outDone);
            if(f)p_CFRelease(f);""",
    """            R13_TEARDOWN_CALL(s,f?f:request,reason,outDone);
            if(f)p_CFRelease(f);""",
    "strip secondary streams before stock teardown")

change(
    """    call_stock_teardown(s,request,reason,outDone);

    if(full_session) {""",
    """    R13_TEARDOWN_CALL(s,request,reason,outDone);

    if(full_session) {""",
    "stock teardown wrapper final path")

src.write_text(s)
print("R13 PASS: guarded real hook /info, 112 isolated SETUP, real AES packet probe,"
      " scoped teardown. DEFAULT OFF. SOURCE ONLY.")
