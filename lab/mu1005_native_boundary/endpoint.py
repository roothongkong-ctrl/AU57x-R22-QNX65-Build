#!/usr/bin/env python3
"""Exact donor-derived markers in selected MU1005 binaries; no runtime claim."""
import hashlib,json,subprocess,sys,uuid
from pathlib import Path
MARKERS={"endpoint_base":b"/dev/iapDevice","channel_class":b"CIapBTChannel",
         "service_class":b"IapServices","device_class":b"IapDevice",
         "path_notification":b"updIapDevicePath",
         "iap2_sdp_uuid":uuid.UUID("00000000-deca-fade-deca-deafdecacaff").bytes}
NAMES={"iap","btstack","bluetooth","libasimmxconnectivity_bluetooth_iapproxy.so","ipod-drvr-iap2.so"}
PINS={"btstack":"51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566",
      "bluetooth":"7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272",
      "ipod-drvr-iap2.so":"42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf"}
def markers(data):
    return {k:data.find(v) for k,v in MARKERS.items() if v in data}
def selftest():
    assert markers(b"xyz/dev/iapDevice\0")=={"endpoint_base":3}
    assert "iap2_sdp_uuid" in markers(b"x"+MARKERS["iap2_sdp_uuid"])
    assert not markers(b"ordinary RFCOMM")
    print("ENDPOINT_MARKER_SELFTEST_OK")
def main(root,out):
    rows=[];seen=set();errors=[]
    for f in sorted(root.rglob("*")):
        if f.name not in NAMES or not f.is_file():continue
        p=f.resolve()
        if not p.is_relative_to(root):continue
        data=p.read_bytes()
        if data[:6]!=b"\x7fELF\x01\x01" or data[18:20]!=b"\x28\x00":continue
        digest=hashlib.sha256(data).hexdigest()
        if (f.name,digest) in seen:continue
        seen.add((f.name,digest))
        if f.name in PINS and digest!=PINS[f.name]:
            continue
        try:
            syms=subprocess.check_output(["readelf","-D","-sW",str(p)],text=True,stderr=subprocess.DEVNULL,timeout=30)
            imports=sorted({s for s in ("resmgr_attach","open64","hid_remote_voldown","hid_remote_volup") if any(s in l.split() and "UND" in l.split() for l in syms.splitlines())})
        except (OSError,subprocess.SubprocessError):
            errors.append(f.name);imports=[]
        rows.append({"name":f.name,"sha256":digest,"marker_offsets":markers(data),"selected_imports":imports})
    present={r["name"] for r in rows}
    report={"scope":"STATIC_MARKERS_ONLY_NO_INSTALL","targets":rows,"missing_targets":sorted(NAMES-present),"scan_errors":errors,"native_endpoint_verified":False,"wireless_verified":False}
    out.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,sort_keys=True))
    if not {"btstack","bluetooth","ipod-drvr-iap2.so"}<=present or errors:raise SystemExit(1)
if __name__=="__main__":
    if sys.argv[1:]==["--selftest"]:selftest()
    else:main(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]))
