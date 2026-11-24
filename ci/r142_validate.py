#!/usr/bin/env python3
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
marks=["R142_BASELINE_BEFORE","R142_PRELOAD","R142_BASELINE_AFTER","@@QNXRUN-END@@"]
for mark in marks:
    if s.count(mark)!=1: raise SystemExit("missing or duplicate marker "+mark)
pos=[s.index(m) for m in marks]
if pos!=sorted(pos): raise SystemExit("unexpected marker order")
for i,required in enumerate([["R137_BASELINE_NO_PRELOAD_OK"],["R137_PRELOAD_CONSTRUCTOR_SAME_PID_OK","R137_QNX_READONLY_SELF_AS_OK"],["R137_BASELINE_NO_PRELOAD_OK"]]):
    b=s[pos[i]:pos[i+1]]
    for m in required:
        if b.count(m)!=1: raise SystemExit("missing proof "+m)
    code=re.findall(r"Process \d+ \(r137_preload_test\) exited status=(\d+)",b)
    if code!=["0"]: raise SystemExit("unexpected process exit "+str(code))
    if "terminated SIG" in b: raise SystemExit("signal")
print("AU57X_R142_QNX_DIRECT_PRELOAD_AND_ROLLBACK_OK_NO_OEM")
