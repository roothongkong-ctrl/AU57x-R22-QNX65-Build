#!/usr/bin/env python3
"""Read the exact public mkdir format used by the unmodified launcher."""
import hashlib,json,re,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
p=Path(sys.argv[1]);raw=p.read_bytes()
pins={"41d523d4f2456237e6ed99bd89a7a8777aff5c2641c2f1663df697934f821813","07955ac794986416fa30122a5964eb8de35f3b93955b1d4d2cb7bc51f7dc3140"}
if hashlib.sha256(raw).hexdigest() not in pins:raise ValueError("launcher identity")
with p.open("rb") as f:
    e=ELFFile(f)
    a=0x101468
    matches=[s for s in e.iter_segments() if s["p_type"]=="PT_LOAD" and s["p_vaddr"]<=a<s["p_vaddr"]+s["p_filesz"]]
    if len(matches)!=1:raise ValueError("format mapping")
    s=matches[0];o=a-s["p_vaddr"];v=s.data()[o:o+160].split(b"\0",1)[0]
    fmt=v.decode("ascii")
    if not fmt or not all(32<=ord(c)<127 for c in fmt):raise ValueError("unexpected literal")
    parent=fmt.rpartition("/")[0] or "."
    if "%" in parent or not re.fullmatch(r"[A-Za-z0-9_./-]{1,100}",parent):raise ValueError("dynamic parent needs review")
    print("ORIGINAL_MKDIR_FORMAT",json.dumps(fmt))
    print("ORIGINAL_MKDIR_PARENT",json.dumps(parent))
    Path(sys.argv[2]).write_text(json.dumps({"format":fmt,"parent":parent,"clock_id":2,"installable":False},indent=2)+"\n")
    Path(sys.argv[3]).write_text("#define DONOR_PARENT "+json.dumps(parent)+"\n")
