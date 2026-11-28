#!/usr/bin/env python3
"""Exact MU1005 SI launch-provider discovery. New evidence, not a live hook."""
import io,json,re,struct,sys,subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_MEM,ARM_REG_PC
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original,sha,imm
PAT=re.compile(r"spawn|execv|system\(|popen|ProcessTNG|ProcessManager|setExecutable|setArguments|startProcess|launchProcess|createProcess|runProcess|waitpid|fork",re.I)
def scan(data):
    e=ELFFile(io.BytesIO(data))
    if not(e.elfclass==32 and e.little_endian and e["e_machine"]=="EM_ARM" and e["e_type"]=="ET_EXEC"):raise ValueError("not pinned ARM executable")
    s=e.get_section_by_name(".text");start=int(s["sh_addr"]);code=s.data()
    loads=[p for p in e.iter_segments() if p["p_type"]=="PT_LOAD"]
    def word(a):
        for p in loads:
            v=int(p["p_vaddr"]);n=int(p["p_filesz"])
            if v<=a and a+4<=v+n:return struct.unpack_from("<I",data,int(p["p_offset"])+a-v)[0]
        return None
    imports={}
    for t in e.iter_sections():
        if not isinstance(t,RelocationSection):continue
        sym=e.get_section(t["sh_link"])
        for r in t.iter_relocations():
            if r["r_info_type"]==22 and r["r_info_sym"]:imports[int(r["r_offset"])]=sym.get_symbol(r["r_info_sym"]).name
    raw=sorted(set(imports.values()))
    dem=dict(zip(raw,subprocess.run(["c++filt"],input="\n".join(raw)+"\n",text=True,capture_output=True,check=True).stdout.splitlines()))
    plt=e.get_section_by_name(".plt");d=plt.data();v=int(plt["sh_addr"]);targets={}
    for o in range(0,len(d)-11,4):
        a,b,c=struct.unpack_from("<III",d,o)
        if a&0xfffff000!=0xe28fc000 or b&0xfffff000!=0xe28cc000 or c&0xfffff000!=0xe5bcf000:continue
        slot=(v+o+8+imm(a)+imm(b)+(c&4095))&0xffffffff
        name=imports.get(slot)
        if name and PAT.search(dem.get(name,name)):targets[v+o]=(name,dem.get(name,name))
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
    def context(a):
        x=max(start,a-40);y=min(start+len(code),a+24)
        return [{"va":hex(i.address),"op":i.mnemonic,"args":i.op_str} for i in md.disasm(code[x-start:y-start],x)]
    strings={}
    for p in loads:
        if p["p_flags"]&2:continue
        buf=p.data();va=int(p["p_vaddr"])
        for m in re.finditer(rb"[\x20-\x7e]{0,150}dio_manager[\x20-\x7e]{0,100}\x00",buf):
            # Only launch-path/name candidates; never export other string contents.
            name=m.group()[:-1]
            if name==b"dio_manager" or name.endswith(b"/dio_manager"):
                strings[va+m.start()]=name.decode("ascii")
    calls=[];refs=[]
    for o in range(0,len(code)-3,4):
        w=struct.unpack_from("<I",code,o)[0];a=start+o
        if w&0x0f000000==0x0b000000 and w>>28!=15:
            rel=(w&0xffffff)<<2
            if rel&0x2000000:rel-=0x4000000
            dst=(a+8+rel)&0xffffffff
            if dst in targets:calls.append({"site":hex(a),"guard":hex(w),"plt":hex(dst),"symbol":targets[dst][1],"context":context(a)})
        # ARM immediate PC-relative literal loads. A direct pointer is evidence;
        # do not label arbitrary register-computed values a proven reference.
        if w&0x0f7f0000==0x051f0000:
            slot=a+8+((w&4095) if w&0x00800000 else -(w&4095))
            val=word(slot)
            if val in strings:refs.append({"site":hex(a),"guard":hex(w),"literal":hex(slot),"target":hex(val),"name":strings[val],"context":context(a)})
    return {"sha256":sha(data),"launch_imports":[{"plt":hex(a),"symbol":n[1]} for a,n in sorted(targets.items())],"calls":calls,"dio_strings":{hex(a):n for a,n in strings.items()},"direct_literal_refs":refs,"runtime_verified":False,"caller_proven":False}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    r=scan(get_original(root,"smartphone_integrator"))
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(r,indent=2)+"\n")
    print("SI_LAUNCH_IMPORTS",json.dumps(r["launch_imports"]))
    print("SI_DIO_STRINGS",json.dumps(r["dio_strings"]))
    print("SI_DIO_REFS",json.dumps(r["direct_literal_refs"]))
    print("SI_LAUNCH_CALLS",len(r["calls"]),json.dumps(r["calls"][:30]))
