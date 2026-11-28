#!/usr/bin/env python3
"""Review actual original DIO / libairplay media and control interfaces.
Report only ELF public symbol/dependency metadata, no original binaries."""
import sys,json,hashlib,re
from pathlib import Path
from elftools.elf.elffile import ELFFile
root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
pins={"dio_manager":"e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c","libairplay.so":"193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5"}
patterns={"video":r"video|screen|display|render|decode|surface","audio":r"audio|sound|volume|microphone","control":r"hid|input|button|knob|steer|touch|keyevent|hmi|iap2","receiver":r"airplayreceiverserver|airplayreceiversession"}
report={}
for name,pin in pins.items():
    found=[f for f in root.rglob(name) if f.is_file() and hashlib.sha256(f.read_bytes()).hexdigest()==pin]
    if not found:raise SystemExit("Missing exact OEM "+name)
    f=sorted(found)[0]
    with f.open("rb") as z:
        e=ELFFile(z)
        assert e.elfclass==32 and e["e_machine"]=="EM_ARM"
        dyn=e.get_section_by_name(".dynamic")
        need=sorted(t.needed for t in dyn.iter_tags("DT_NEEDED")) if dyn else []
        ds=e.get_section_by_name(".dynsym")
        syms=[(s.name,s["st_shndx"]=="SHN_UNDEF") for s in ds.iter_symbols() if s.name] if ds else []
        groups={}
        for k,pat in patterns.items():
            groups[k]={"imports":sorted(set(n for n,u in syms if u and re.search(pat,n,re.I))),
                       "exports":sorted(set(n for n,u in syms if not u and re.search(pat,n,re.I)))}
        report[name]=dict(path=str(f.relative_to(root)),sha256=pin,needed=need,groups=groups)
assert "libairplay.so" in report["dio_manager"]["needed"]
for n in ("AirPlayReceiverServerControl","AirPlayReceiverServerSetProperty"):
    assert n in report["libairplay.so"]["groups"]["receiver"]["exports"],n
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(dict(original=report,actual_media_verified=False,installable=False),indent=2)+"\n")
for name,info in report.items():
    print("ORIGINAL_MEDIA_DEPENDENCIES",name,json.dumps(info["needed"]))
    for k,g in info["groups"].items():
        print("ORIGINAL_MEDIA_SYMBOLS",name,k,"imports",len(g["imports"]),"exports",len(g["exports"]),json.dumps((g["imports"]+g["exports"])[:18]))
print("OEM_DIO_AIRPLAY_DEPENDENCY_VERIFIED_STATIC_ONLY")
