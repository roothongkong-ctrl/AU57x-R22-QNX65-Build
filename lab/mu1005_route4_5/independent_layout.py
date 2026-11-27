#!/usr/bin/env python3
"""Reuse existing exact-MU1005 launch staging. Private firmware input only.
Only the new wrapper and metadata are emitted; no OEM config is published.
The opt-in preload is diagnostic only, not a wireless service activation."""
import hashlib,json,sys,subprocess
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
import mu1005_efs_launcher_stage as stock
DIR="/mnt/app/root/au57x-wireless"
WRAPPER=DIR+"/au57x_btstack_probe_entry.sh"
PRELOAD=DIR+"/libmu1005_bt_preload_review.so"
def stage(c):
    old=stock.stage_config(c)
    if old.count(stock.WRAPPER.encode())!=1:raise ValueError("ambiguous launch slot")
    new=old.replace(stock.WRAPPER.encode(),WRAPPER.encode(),1)
    if new.replace(WRAPPER.encode(),stock.STOCK.encode(),1)!=c:raise ValueError("not single-field reversible")
    parsed=stock.parse(c);parsed["launcher"]["btstack"]["exec"]=WRAPPER
    if stock.parse(new)!=parsed:raise ValueError("launch semantics changed")
    script=stock.WRAPPER_TEXT.replace(stock.PRELOAD,PRELOAD)
    if stock.PRELOAD in script or not script.endswith('exec /eso/bin/apps/btstack "$@"\n'):raise ValueError("stock exec not preserved")
    subprocess.run(["sh","-n"],input=script.encode(),check=True)
    return new,script
if __name__=="__main__":
    c=Path(sys.argv[1]).read_bytes();new,script=stage(c)
    out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=False)
    (out/"au57x_btstack_probe_entry.sh").write_text(script)
    report={"scope":"EXACT_ORIGINAL_CONFIG_SINGLE_FIELD_OFFLINE","directory_candidate":DIR,"config_original_sha256":stock.sha(c),"config_candidate_sha256":stock.sha(new),"changed_field":"launcher.btstack.exec","stock_executable":stock.STOCK,"candidate_wrapper":WRAPPER,"probe_library":PRELOAD,"default_exec_stock":True,"stock_config_reversible":True,"stock_failure_actions_preserved":True,"original_binaries_replaced":False,"actual_factory_launch_verified":False,"wireless_activation_bound":False,"installable":False}
    (out/"STATUS.json").write_text(json.dumps(report,indent=2)+"\n")
    print("ROUTE6_INDEPENDENT_LAYOUT_EXACT_STOCK_SINGLE_FIELD_NOT_RUNTIME")
