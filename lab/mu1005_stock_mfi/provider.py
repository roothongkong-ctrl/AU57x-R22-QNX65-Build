#!/usr/bin/env python3
"""Identify the exact original MFi library and its actual ELF consumers.
Read only static ELF metadata; export no OEM files, credentials or memory."""
import json,sys,hashlib
from pathlib import Path
from elftools.elf.elffile import ELFFile
ROOT=Path(sys.argv[1]).resolve(strict=True)
OUT=Path(sys.argv[2])
PIN="193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5"
NAMES={"APSMFiPlatform_CopyCertificate","APSMFiPlatform_CreateSignature"}
providers=[]; consumers=[]; edges=[]
for f in ROOT.rglob("*"):
    if not f.is_file() or f.is_symlink():continue
    try:
        with f.open("rb") as z:
            if z.read(4)!=b"\x7fELF":continue
            z.seek(0);e=ELFFile(z)
            if e.elfclass!=32 or e["e_machine"]!="EM_ARM":continue
            dyn=e.get_section_by_name(".dynamic")
            needed=[t.needed for t in dyn.iter_tags("DT_NEEDED")] if dyn else []
            ds=e.get_section_by_name(".dynsym")
            syms=[s for s in ds.iter_symbols() if s.name in NAMES] if ds else []
            defined={s.name:hex(int(s["st_value"])) for s in syms if s["st_shndx"]!="SHN_UNDEF"}
            imported=sorted(s.name for s in syms if s["st_shndx"]=="SHN_UNDEF")
            path=str(f.relative_to(ROOT))
            if defined:
                z.seek(0);digest=hashlib.sha256(z.read()).hexdigest()
                providers.append(dict(path=path,sha256=digest,pinned=digest==PIN,exports=defined))
            if imported or any(n.startswith("libairplay.so") for n in needed):
                consumers.append(dict(path=path,imports=imported,needed=[n for n in needed if n.startswith("libairplay.so")]))
            if needed:edges.append((path,needed))
    except (OSError,ValueError) as ex:
        print("ELF_READ_ERROR",str(f.relative_to(ROOT)),type(ex).__name__)
# Report exact one-hop consumers of the actual authentication importers.
# DT_NEEDED evidence is not a live loader/owner or a completed call.
parents=[]
for row in consumers:
    if not row["imports"]:continue
    name=Path(row["path"]).name
    for path,needed in edges:
        if name in needed:parents.append(dict(path=path,via=name))
report=dict(providers=providers,consumers=consumers,parents=parents,
            runtime_verified=False,installable=False)
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text(json.dumps(report,indent=2)+"\n")
print("MFI_PROVIDERS",json.dumps(providers))
print("MFI_CONSUMERS",json.dumps(consumers))
print("MFI_PARENT_EDGES",json.dumps(parents))
assert any(x["pinned"] and set(x["exports"])==NAMES for x in providers), "Exact historical MFi library not found in this app70 extraction"
