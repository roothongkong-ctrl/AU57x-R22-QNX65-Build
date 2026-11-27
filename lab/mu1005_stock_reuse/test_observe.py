#!/usr/bin/env python3
import os,subprocess,tempfile
from pathlib import Path
probe=Path(__file__).with_name("observe.sh")
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/"pidin"
 p.write_text("""#!/bin/sh
case "$2" in
 dio_manager) printf '99 /dev/i2c8 /dev/i2c8 /dev/ipod0 secret=DO_NOT_LOG\\n';;
 connectivity_launcher) printf '21 /mnt/system/etc/eso/production/connectivity.json\\n';;
 mdnsd) printf '88 /var/run/mDNSResponder\\n';;
 *) exit 1;;
esac
""")
 p.chmod(0o755)
 r=subprocess.run(["sh",str(probe)],env={**os.environ,"PATH":d+":"+os.environ["PATH"]},capture_output=True,text=True,check=True)
 s=r.stdout
 assert s.count("FD_CANDIDATE dio_manager /dev/i2c8")==1
 assert "FD_CANDIDATE dio_manager /dev/ipod0" in s
 assert "FD_CANDIDATE connectivity_launcher /mnt/system/etc/eso/production/connectivity.json" in s
 assert "FD_CANDIDATE mdnsd /var/run/mDNSResponder" in s
 assert "DO_NOT_LOG" not in s and "PROCESS_QUERY_UNAVAILABLE btstack" in s
 assert "STOCK_PROVIDER_OBSERVATION_ONLY_NO_AUTH_OR_OWNER_PROOF" in s
print("NEW_COMBINED_OBSERVER_FILTER_TEST_PASS_NOT_OEM")
