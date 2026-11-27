#!/usr/bin/env python3
from pathlib import Path
import re,sys
root=Path(sys.argv[1])
serial=list(root.rglob("serial.log"))
assert len(serial)==1,serial
s=serial[0].read_text(errors="replace")
assert s.count("@@QNXRUN-START@@")==1 and s.count("@@QNXRUN-END@@")==1
body=s.split("@@QNXRUN-START@@",1)[1].split("@@QNXRUN-END@@",1)[0]
for marker in ("PASS 5_TO_24_REFRESH_BEFORE_ASSOCIATION_NO_OEM","PASS 24_TO_5_REFRESH_BEFORE_ASSOCIATION_NO_OEM","R105152_QNX_LIVE_AP_BOTH_DIRECTIONS_FIXTURE_PASS"):
 assert marker in body.splitlines(),repr(marker)
 assert body.splitlines().count(marker)==1
assert re.search(r"Process \d+ \(mu1005-live-ap\) exited status=0(?:\.|\s|$)",s)
assert not re.search(r"Process \d+ \(mu1005-live-ap\) terminated SIG",s)
print("R105154_RETAINED_QNX_DUAL_BAND_CONFIRMED_FIXTURE_ONLY")
