#!/usr/bin/env python3
"""Resolve DIO key10 and actual device-node configuration, including extensionless files.
Read only; never treats a wired endpoint as proof of a wireless transport."""
import io,sys,struct,json,re
from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original,sha
root=Path(sys.argv[1]).resolve();d=get_original(root,"dio_manager")
e=ELFFile(io.BytesIO(d));segs=[(int(s["p_vaddr"]),s.data()) for s in e.iter_segments() if s["p_type"]=="PT_LOAD"]
def at(a,n):
    for v,b in segs:
        if v<=a and a+n<=v+len(b):return b[a-v:a-v+n]
    return b""
def word(a):
    x=at(a,4)
    if len(x)!=4:raise ValueError("unmapped "+hex(a))
    return struct.unpack("<I",x)[0]
def literal(a):
    v=at(a,160).split(b"\0",1)[0]
    if v and len(v)<160 and re.fullmatch(rb"[A-Za-z0-9_./-]{1,150}",v):
        return v.decode()
    return None
# Exact PC-relative default string reconstruction from R105.321.
fallback=(0x130bcc+word(0x130bf4)+word(0x130bf8))&0xffffffff
print("DIO_ACCESSOR_DEFAULT",hex(fallback),json.dumps(literal(fallback)))
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
lookup=[{"va":hex(i.address),"op":i.mnemonic,"args":i.op_str} for i in md.disasm(at(0x130884,0x180),0x130884)]
print("DIO_KEY_LOOKUP",json.dumps(lookup))
# Search only known public config identifiers and device paths; do not emit raw files.
pat=re.compile(rb"(?i)(?:iap2[./_]device|/dev/(?:ipod|iap)[A-Za-z0-9_.-]{0,80})")
paths=[];config=[];provider=[]
for p in root.rglob("*"):
    if p.is_symlink() or not p.is_file():continue
    size=p.stat().st_size
    if size>24*1024*1024 or p.suffix in (".img",".imagefs"):continue
    try:raw=p.read_bytes()
    except OSError:continue
    matches=sorted({m.group().decode("ascii","replace") for m in pat.finditer(raw)})
    if not matches:continue
    rel=str(p.relative_to(root))
    if raw.startswith(b"\x7fELF"):
        provider.append({"file":rel,"public_identifiers":matches})
        continue
    # Configuration may be named dio_manager without a suffix.
    if b"\0" not in raw and size<=2*1024*1024:
        config.append({"file":rel,"public_identifiers":matches})
        for m in re.finditer(rb'(?i)["\']?(?:iap2[./_]device|device)["\']?\s*[:=]\s*["\']?(/dev/(?:iap|ipod)[A-Za-z0-9_.-]{0,80})',raw):
            paths.append({"file":rel,"device":m.group(1).decode()})
# Actual configuration values only, not assumed wireless-capable.
out={"sha256":sha(d),"accessor_default":{"address":hex(fallback),"string":literal(fallback)},"lookup_instructions":lookup,"config_matches":config,"configured_paths":paths,"binary_candidates":provider,"runtime_verified":False,"installable":False}
for k in ("config_matches","configured_paths","binary_candidates"):print("DIO_"+k.upper(),json.dumps(out[k]))
Path(sys.argv[2]).write_text(json.dumps(out,indent=2)+"\n")
