#!/usr/bin/env python3
"""After six MU1005 adapters: explicit child startup acknowledgement."""
from pathlib import Path
import subprocess,sys
up=Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
p=up/"wireless/daemon/src/main.rs"
s=p.read_text()
anchor="        let start = Instant::now();"
assert s.count(anchor)==1
s=s.replace(anchor,'''        // In inherited mode, startup is not ready merely because TCP exists.
        // Send before any RFCOMM stream reads, after configuration, receiver
        // worker and (on QNX) real AP IE initialization. Not MFi success.
        if inherited && std::env::var("MU1005_BRIDGE_READY").as_deref()==Ok("1") {
            use std::io::Write;
            stream.write_all(b"MU1005-READY-v1\\n")?;
        }
'''+anchor)
p.write_text(s)
print("MU1005_EXPLICIT_CHILD_READY_SOURCE_ADAPTED")
