#!/usr/bin/env python3
"""Read-only analysis of genuine CarLife native EA provider, not an iAP auth substitute."""
from pathlib import Path
import hashlib,io,json,sys,re,struct,subprocess
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2])
rx=re.compile(r"(?i)resmgr|iofunc|dispatch|iap2|nativeea|bluetooth|rfcomm|socket|listen|accept|connect|transport|carplay|carlife|usb")
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
rows=[]
for p in root.rglob("*"):
    if p.name not in ("carlife","iap_nativeea") or not p.is_file() or p.is_symlink():continue
    raw=p.read_bytes()
    if not raw.startswith(b"\x7fELF"):continue
    e=ELFFile(io.BytesIO(raw))
    if e.elfclass!=32 or e["e_machine"]!="EM_ARM" or not e.little_endian:continue
    segs=[(int(s["p_vaddr"]),s.data()) for s in e.iter_segments() if s["p_type"]=="PT_LOAD"]
    sec=e.get_section_by_name(".text")
    def word(a):
        for v,d in segs:
            if v<=a and a+4<=v+len(d):return struct.unpack_from("<I",d,a-v)[0]
        return None
    dyn=e.get_section_by_name(".dynamic")
    deps=[t.needed for t in dyn.iter_tags() if t.entry.d_tag=="DT_NEEDED"] if dyn else []
    syms=[]
    for t in e.iter_sections():
        if not isinstance(t,SymbolTableSection):continue
        for s in t.iter_symbols():
            if not s.name or not rx.search(s.name):continue
            syms.append({"table":t.name,"symbol":s.name,"value":hex(s["st_value"]),"undefined":s["st_shndx"]=="SHN_UNDEF"})
    nodes={}
    for v,d in segs:
        for m in re.finditer(rb"/dev/(?:iap2native|ipod0)\x00",d):
            nodes[v+m.start()]=m.group()[:-1].decode()
    refs=[]
    if sec:
        base=int(sec["sh_addr"]);code=sec.data()
        for off in range(0,len(code)-3,4):
            a=base+off;w=struct.unpack_from("<I",code,off)[0]
            if w&0x0f7f0000!=0x051f0000:continue
            offset=w&0xfff;val=word(a+8+(offset if w&0x800000 else -offset))
            if val is None:continue
            if val in nodes:refs.append({"site":hex(a),"node":nodes[val],"kind":"literal"})
            reg=(w>>12)&15
            # Recognize the common ARM PIC add Rd,PC,Rm in a bounded window.
            for k in range(off+4,min(off+48,len(code)-3),4):
                v=struct.unpack_from("<I",code,k)[0]
                if v&0x0fe00ff0==0x00800000 and ((v>>16)&15)==15 and (v&15)==reg:
                    dest=(base+k+8+val)&0xffffffff
                    if dest in nodes:refs.append({"site":hex(base+k),"node":nodes[dest],"kind":"pc-relative-candidate"})
                if v&0x0e000000==0x0a000000:break
    row={"file":str(p.relative_to(root)),"sha256":hashlib.sha256(raw).hexdigest(),"needed":deps,"symbols":syms,"nodes":{hex(k):v for k,v in nodes.items()},"references":refs}
    rows.append(row)
    print("NATIVE_EA_MODULE",row["file"],"deps",json.dumps(deps),"nodes",json.dumps(row["nodes"]))
    print("NATIVE_EA_SYMBOLS",json.dumps(syms)[:18000])
    print("NATIVE_EA_REFERENCES",json.dumps(refs))
if len(rows)<2:raise ValueError("original native EA modules incomplete")
out.write_text(json.dumps({"modules":rows,"scope":"static endpoint classification only; native EA does not imply CarPlay or wireless","runtime_verified":False,"installable":False},indent=2)+"\n")
