#!/usr/bin/env python3
"""Extract only allowlisted DIO executable path witnesses from stock debug scripts."""
import json,re,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
rows=[]
for name in ("CP_audio_dumping.sh","CP_video_dumping.sh"):
    for f in root.rglob(name):
        if not f.is_file() or not f.resolve().is_relative_to(root):continue
        for n,line in enumerate(f.read_text(errors="replace").splitlines(),1):
            for m in re.finditer(r"(?<![\w/])(?:/mnt/app/eso/bin/apps/|/eso/bin/apps/)?dio_manager(?![\w])",line):
                kind="pid-query" if re.search(r"\b(pidin|pidof|slay)\b",line) else "reference"
                rows.append({"script":name,"line":n,"token":m.group(),"kind":kind})
print("R105288_STOCK_SCRIPT",json.dumps(rows))
Path(sys.argv[2]).write_text(json.dumps({"references":rows,"runtime_verified":False,"installable":False},indent=2)+"\n")
