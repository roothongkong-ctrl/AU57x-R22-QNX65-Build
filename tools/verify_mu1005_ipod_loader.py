#!/usr/bin/env python3
"""Read-only pinned MU1005 mm-ipod loader static audit; no OEM upload."""
import argparse,hashlib,json,sys
from pathlib import Path
EXPECTED="4b87cd2d41ddcee2f38800feafe8b4b340e4f23e1d86c7e97af303522aa15d09"
MARKERS=["device_transport_init","ipod-transport-%s.so",
         "ipod-drvr-%s.so","/dev/ipod","usbdevice,usbhost","dlopen","dlsym"]
def audit(path):
    r={"firmware":"MHI2_CN_AU57x_K1006_2/MU1005","sample":"mm-ipod",
       "sha256_match":False,"runtime_selected_transport_proven":False,
       "bluetooth_transport_plugin_found":False,
       "bluetooth_to_mm_ipod_link_proven":False,"vehicle_installable":False}
    if not path.is_file():
        r["verdict"]="BLOCKED_MISSING_OEM_SAMPLE";return r
    data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
    present={k:k.encode("ascii") in data for k in MARKERS}
    valid=(digest==EXPECTED and all(present.values()))
    r.update(sha256=digest,sha256_match=(digest==EXPECTED),
             static_symbols_and_strings=present,static_7_of_7=all(present.values()),
             verdict="PASS_STATIC_LOADER_SAMPLE_ONLY" if valid
             else "BLOCKED_OEM_SAMPLE_MISMATCH")
    return r
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--mm-ipod",required=True,type=Path)
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    r=audit(args.mm_ipod)
    data=json.dumps(r,indent=2,ensure_ascii=False)+"\n"
    if args.output:args.output.write_text(data,encoding="utf-8")
    print(data)
    return 0 if r["verdict"]=="PASS_STATIC_LOADER_SAMPLE_ONLY" else 20
if __name__=="__main__":sys.exit(main())
