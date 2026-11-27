#!/usr/bin/env python3
import json,re,sys
from pathlib import Path
expected=json.loads(Path(sys.argv[1]).read_text())
log=Path(sys.argv[2]).read_text()
rows=re.findall(r"^DEP_RESULT ([A-Za-z0-9_.+-]+) (EXIT_\d+|SIGNAL_\d+|TIMEOUT|WAIT_FAIL|SPAWN_FAIL|OTHER)$",log,re.M)
assert [n for n,_ in rows]==expected["direct_needed"],"missing/duplicate/out of order dependency results"
assert "DEP_END_NOT_SERVICE_READY" in log
success=set(re.findall(r"^DEP_LOAD_OK ([A-Za-z0-9_.+-]+)$",log,re.M))
report={"program":expected["program"],"scope":"DIRECT_DEPENDENCY_ISOLATION_NOT_OEM_SERVICE","rows":[{"library":n,"process_result":r,"dlopen_ok":n in success} for n,r in rows],"wireless_verified":False,"service_ready":False}
Path(sys.argv[3]).write_text(json.dumps(report,indent=2)+"\n")
print("DEP_ISOLATION_COMPLETE",expected["program"],len(rows))
