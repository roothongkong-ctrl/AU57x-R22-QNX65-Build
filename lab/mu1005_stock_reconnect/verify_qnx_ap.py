#!/usr/bin/env python3
"""Validate original QNX serial evidence, allowing CRLF line endings only."""
import re
import sys
from pathlib import Path
d=Path(sys.argv[1])
runtime=(d/"runtime.log").read_bytes().replace(b"\r\n",b"\n")
serial=(d/"serial.log").read_bytes().replace(b"\r\n",b"\n")
start=b"@@QNXRUN-START@@"
end=b"@@QNXRUN-END@@"
if serial.count(start)!=1 or serial.count(end)!=1 or serial.index(start)>=serial.index(end):
    raise SystemExit("Missing or ambiguous QNX program start/end")
body=serial.split(start,1)[1].split(end,1)[0]
markers=(b"PASS 5_TO_24_REFRESH_BEFORE_ASSOCIATION_NO_OEM",
         b"PASS 24_TO_5_REFRESH_BEFORE_ASSOCIATION_NO_OEM",
         b"R105152_QNX_LIVE_AP_BOTH_DIRECTIONS_FIXTURE_PASS")
for m in markers:
    if m not in runtime.split(b"\n") or m not in body.split(b"\n"):
        raise SystemExit("Missing exact QNX fixture marker: "+m.decode())
    print(m.decode())
codes=re.findall(rb"Process [0-9]+ \(mu1005-live-ap\) exited status=([0-9]+)\b",serial)
if codes!=[b"0"] or re.search(rb"Process [0-9]+ \(mu1005-live-ap\) terminated SIG",serial):
    raise SystemExit("Missing or nonzero actual QNX exit status")
print("QNX_DUAL_BAND_RUNTIME_EVIDENCE_PASS_EXIT_0_FIXTURE_ONLY")
