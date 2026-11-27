#!/usr/bin/env python3
"""R105.166 step8: evidence at UnicodeString source, Provider arguments, exact copy contract."""
import hashlib,json,sys,copy
from pathlib import Path
root=Path(sys.argv[2]).resolve(strict=True);out=Path(sys.argv[3]);lane=sys.argv[1]
if lane=="reader-object":
 sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_launcher_next"))
 from next import original,windows,PIN
 b=original(root)
 sites={"unicode_utf8":0x117958,"parser_b":0x1179ac,"provider_ctor":0x1280f0,"provider_format":0x1280a8}
 report={"sha256":PIN,"sites":{k:windows(b,v-80,v+16) for k,v in sites.items()},"scope":"ORIGINAL_OBJECT_AND_REGISTER_ORIGINS_ONLY","runtime_file_association":False}
elif lane=="config-contract":
 sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
 from mu1005_efs_launcher_stage import parse,stage_config,CONFIG_SHA,sha,WRAPPER
 p=root/"stage2/filesystems/MMX2/efs-sys/70/default/efs-system.img/etc/eso/production/connectivity.json"
 original=p.read_bytes();assert sha(original)==CONFIG_SHA
 a=parse(original);b=parse(stage_config(original))
 expected=copy.deepcopy(a);expected["launcher"]["btstack"]["exec"]=WRAPPER
 assert b==expected and type(a["bluetooth"]["enableIap"]) is bool
 assert a["launcher"]["applications"]==b["launcher"]["applications"]
 report={"scope":"EXACT_CONFIG_SEMANTIC_COMPARISON_HOST_ONLY","root_keys":sorted(a),"launcher_keys":sorted(a["launcher"]),"only_exec_changed":True,"original_sha256":sha(original),"candidate_sha256":sha(stage_config(original)),"native_parser_verified":False}
else:raise ValueError(lane)
report["installable"]=False
out.write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,sort_keys=True))
