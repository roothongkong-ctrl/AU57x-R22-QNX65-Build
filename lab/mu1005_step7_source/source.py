#!/usr/bin/env python3
"""Step7 targeted original argument-source evidence; not a runtime proof."""
import json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_launcher_next"))
from next import original,windows,calls_to,Binary
from probe import load
lane,root,out=sys.argv[1:]
root=Path(root).resolve(strict=True)
if lane=="launcher-argv-api":
 b=original(root)
 imports={v:n for v,n in b.plt.items() if re.search(r"ProcessTNG|ProcessArguments|ArgumentList|(^|_)(spawn[vlep]*|exec[vlep]*|posix_spawn[p]?)(@|$)",n)}
 selected={v:n for v,n in imports.items() if re.search(r"arg|param|command|start|launch|spawn|exec",n,re.I) and "setExecutable" not in n}
 hits=calls_to(b,set(selected))
 result={"imports":{hex(v):n for v,n in imports.items()},"argument_launch_candidates":{hex(v):n for v,n in selected.items()},"calls":hits[:120],"context":[{"call":r,"window":windows(b,int(r["va"],16)-64,int(r["va"],16)+24)} for r in hits[:16]]}
elif lane=="btstack-argv-consumer":
 b=Binary(load(root,"btstack"))
 imports={v:n for v,n in b.plt.items() if re.search(r"(^|_)(getopt(_long)?|atoi|strtol|strtoul|strtoll|sscanf|__libc_start_main)(@|$)|parseCommandLine|parseArguments|CommandLineParser",n,re.I)}
 hits=calls_to(b,set(imports))
 result={"imports":{hex(v):n for v,n in imports.items()},"entry":hex(int(b.e["e_entry"])),"entry_window":windows(b,int(b.e["e_entry"]),int(b.e["e_entry"])+128),"main_symbols":[{"va":hex(v),"name":n,"size":s} for v,s,n in b.funcs if n=="main"],"calls":hits[:100],"context":[{"call":r,"window":windows(b,int(r["va"],16)-48,int(r["va"],16)+20)} for r in hits[:12]]}
else:raise ValueError(lane)
report={"lane":lane,"scope":"STATIC_ARGUMENT_SOURCE_CANDIDATES_ONLY","result":result,"runtime_argument_source_verified":False,"installable":False}
Path(out).write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,sort_keys=True))
print("R105160_STEP7_SOURCE_OBSERVATION_ONLY")
