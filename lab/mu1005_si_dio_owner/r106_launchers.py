#!/usr/bin/env python3
"""Trace actual app70 launch candidates beyond SI, informed by R106 wired capture.
No OEM file contents, credentials or process arguments are exported."""
import io,json,sys,hashlib
from pathlib import Path
from elftools.elf.elffile import ELFFile
from launch_dataflow import analyze
from launch_provider import scan
root=Path(sys.argv[1]).resolve(strict=True); out=Path(sys.argv[2])
names=("connectivity_launcher","servicemgrmibhigh","bluetooth","connectionmanager","dio_manager")
rows=[]
for name in names:
    candidates=[p for p in root.rglob(name) if p.is_file() and p.resolve().is_relative_to(root)]
    for f in sorted(candidates):
        data=f.read_bytes()
        if not data.startswith(b"\x7fELF"):continue
        e=ELFFile(io.BytesIO(data))
        if e.elfclass!=32 or e["e_machine"]!="EM_ARM" or e["e_type"]!="ET_EXEC" or not e.get_section_by_name(".plt"):continue
        result=scan(data)
        refs=analyze(data) if result["dio_strings"] else {}
        row={"program":name,"path":str(f.relative_to(root)),"sha256":hashlib.sha256(data).hexdigest(),
             "launch_imports":result["launch_imports"],"launch_calls":result["calls"],
             "dio_direct_refs":result["direct_literal_refs"],"dio_strings":result["dio_strings"],
             "computed_refs":refs.get("computed_refs",[]),"argument_candidates":refs.get("call_argument_candidates",[])}
        rows.append(row)
        print("R106_LAUNCH_CANDIDATE",name,"calls",len(row["launch_calls"]),"names",len(row["dio_strings"]),"refs",len(row["dio_direct_refs"])+len(row["computed_refs"]),"arguments",json.dumps(row["argument_candidates"])[:5000])
if not rows:raise SystemExit("No exact firmware process candidates")
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({"historical_r106":"USB DIO and carplay0 observed, parent not recorded","programs":rows,"runtime_verified":False,"caller_proven":False,"installable":False},indent=2)+"\n")
