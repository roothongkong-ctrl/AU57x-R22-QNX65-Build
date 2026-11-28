#!/usr/bin/env python3
"""Locate real SI/DIO USB completion and identity providers in exact MU1005.
Only function names, addresses, call relationships and instruction guards.
No firmware output or inferred wireless authorization."""
import hashlib, io, json, re, struct, subprocess, sys
from pathlib import Path
from bisect import bisect_right
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from elftools.elf.relocation import RelocationSection
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
PINS={"smartphone_integrator":"2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b",
      "dio_manager":"e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c"}
PAT=re.compile(r"(usb|wired|disconnect|device.?lost|device.?removed|deactivat|stopcontroller|stopservice|reportconnection|reportparing|reportpairing|localbluetoothaddress|bluetoothaddress|deviceaddress|deviceidentifier|serialnumber|devicelist|waitpid|posix_spawn|spawn|execv|process.?exit|child.?exit)",re.I)
def sha(d):return hashlib.sha256(d).hexdigest()
def get_original(root,name):
    for p in root.rglob(name):
        if p.is_file() and p.resolve().is_relative_to(root):
            d=p.read_bytes()
            if sha(d)==PINS[name]:return d
    raise ValueError("Exact firmware file unavailable: "+name)
def imm(w):
    r=((w>>8)&15)*2;n=w&255
    return ((n>>r)|(n<<(32-r)))&0xffffffff if r else n
def examine(data,name):
    e=ELFFile(io.BytesIO(data))
    assert e.elfclass==32 and e.little_endian and e["e_machine"]=="EM_ARM"
    s=e.get_section_by_name(".text");start=int(s["sh_addr"]);code=s.data();end=start+len(code)
    names={}; funcs={};imports={}
    for tab in e.iter_sections():
        if not isinstance(tab,SymbolTableSection):continue
        for v in tab.iter_symbols():
            a=int(v["st_value"]);n=int(v["st_size"])
            if not v.name or v["st_info"]["type"]!="STT_FUNC" or not start<=a<end:continue
            names.setdefault(a,set()).add(v.name)
            if n and a+n<=end:funcs[a]=max(n,funcs.get(a,0))
    for tab in e.iter_sections():
        if not isinstance(tab,RelocationSection):continue
        sym=e.get_section(tab["sh_link"])
        for r in tab.iter_relocations():
            if r["r_info_type"]==22 and r["r_info_sym"]:
                imports[int(r["r_offset"])]=sym.get_symbol(r["r_info_sym"]).name
    plt=e.get_section_by_name(".plt")
    if plt:
        d=plt.data();v=int(plt["sh_addr"])
        for off in range(0,len(d)-11,4):
            a,b,c=struct.unpack_from("<III",d,off)
            if a&0xfffff000!=0xe28fc000 or b&0xfffff000!=0xe28cc000 or c&0xfffff000!=0xe5bcf000:continue
            slot=(v+off+8+imm(a)+imm(b)+(c&4095))&0xffffffff
            if slot in imports:names.setdefault(v+off,set()).add(imports[slot]+"@plt")
    raw=sorted({n for ns in names.values() for n in ns})
    dem=subprocess.run(["c++filt"],input="\n".join(raw)+"\n",text=True,check=True,capture_output=True).stdout.splitlines()
    readable=dict(zip(raw,dem))
    targets={a:[readable[n] for n in sorted(ns) if PAT.search(readable[n])] for a,ns in names.items()}
    targets={a:ns for a,ns in targets.items() if ns}
    bounds=sorted(funcs)
    def owner(a):
        i=bisect_right(bounds,a)-1
        if i>=0 and a<bounds[i]+funcs[bounds[i]]:
            b=bounds[i]
            return {"entry":hex(b),"name":[readable.get(n,n) for n in sorted(names.get(b,[]))]}
        return None
    # Exact SI reset anchor from existing STATE_EVIDENCE. Do not classify
    # reset as USB until a USB-specific caller/phone association is proven.
    reset=0x188018 if name=="smartphone_integrator" else None
    reset_owner=owner(reset) if reset else None
    reset_entry=int(reset_owner["entry"],16) if reset_owner else None
    if reset_entry is not None:targets.setdefault(reset_entry,["known SI reset-containing function (candidate)"])
    callers={}
    for o in range(0,len(code)-3,4):
        w=struct.unpack_from("<I",code,o)[0]
        if w&0x0f000000!=0x0b000000 or w>>28==15:continue
        rel=(w&0xffffff)<<2
        if rel&0x2000000:rel-=0x4000000
        a=start+o;target=(a+8+rel)&0xffffffff
        if target in targets:
            row={"site":hex(a),"caller":owner(a),"guard":hex(w)}
            callers.setdefault(target,[]).append(row)
    # Also retain entry words for selected definitions, not PLT stubs.
    # This permits later exact-image guard generation without copying ELF.
    rows=[]
    for a,ns in sorted(targets.items()):
        words=[]
        if start<=a and a+16<=end:words=[hex(x) for x in struct.unpack_from("<4I",code,a-start)]
        cs=callers.get(a,[])
        rows.append({"entry":hex(a),"symbols":ns,"defined":start<=a<end,
            "entry_guards":words,"caller_count":len(cs),"callers":cs[:30]})
    reset_ins=[]
    if reset and start<=reset and reset+28<=end:
        md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
        reset_ins=[{"va":hex(i.address),"op":i.mnemonic,"args":i.op_str} for i in md.disasm(code[reset-start:reset-start+28],reset)]
    return {"sha256":sha(data),"symbol_count":len(names),"candidates":rows,
            "reset_owner":reset_owner,"reset_instructions":reset_ins}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    report={name:examine(get_original(root,name),name) for name in PINS}
    out.write_text(json.dumps({"scope":"EXACT_STATIC_USB_LIFECYCLE_AND_IDENTITY_CANDIDATES","installable":False,"runtime_verified":False,"programs":report},indent=2)+"\n")
    for name,r in report.items():
        print(name,"candidates",len(r["candidates"]),"reset_owner",r["reset_owner"])
        for row in r["candidates"]:
            if row["caller_count"] or row["defined"]:
                print(row["entry"],"; ".join(row["symbols"]), "callers",row["caller_count"])
