#!/usr/bin/env python3
"""Resolve real filesystem prerequisites from the public launcher, read-only."""
import hashlib,json,re,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
PIN="41d523d4f2456237e6ed99bd89a7a8777aff5c2641c2f1663df697934f821813"
p=Path(sys.argv[1])/"standalone/SD_ROOT/carplay/payload/wireless/profiles/AID10.5-17PM/wireless-receiver-exec"
raw=p.read_bytes()
if hashlib.sha256(raw).hexdigest()!=PIN:raise ValueError("unexpected input")
with p.open("rb") as f:
    e=ELFFile(f);syms={}
    for s in e.iter_sections():
        if isinstance(s,SymbolTableSection):
            for x in s.iter_symbols():
                if x["st_info"]["type"]=="STT_FUNC" and x["st_shndx"]!="SHN_UNDEF" and x["st_value"]:
                    syms[int(x["st_value"])]=(x.name,int(x["st_size"]))
    rel=e.get_section_by_name(".rel.plt");plt=e.get_section_by_name(".plt")
    if rel and plt:
        table=e.get_section(rel["sh_link"])
        for i,x in enumerate(rel.iter_relocations()):
            syms[int(plt["sh_addr"])+20+12*i]=("PLT_CANDIDATE:"+table.get_symbol(x["r_info_sym"]).name,12)
    def data_at(a,n):
        for s in e.iter_segments():
            if s["p_type"]=="PT_LOAD" and int(s["p_vaddr"])<=a<int(s["p_vaddr"])+int(s["p_filesz"]):
                o=a-int(s["p_vaddr"]);return s.data()[o:o+n]
        return b""
    def path_at(a):
        x=data_at(a,160).split(b"\0",1)[0]
        # Only actual public filesystem paths and their format strings.
        if re.fullmatch(rb"/(?:var|mnt|dev|etc|proc|tmp|eso|run)/[A-Za-z0-9_./%+ -]{1,150}",x):
            return x.decode()
        return None
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);rows={}
    for a,(name,size) in sorted(syms.items()):
        if name not in ("main","hash_file") or not size:continue
        regs={};lines=[]
        for i in md.disasm(data_at(a,size),a):
            line={"addr":hex(i.address),"op":i.mnemonic,"args":i.op_str}
            m=re.fullmatch(r"(\w+), #(0x[0-9a-f]+|\d+)",i.op_str)
            if m and i.mnemonic in ("movw","movt"):
                reg,val=m.group(1),int(m.group(2),0)
                regs[reg]=val if i.mnemonic=="movw" else (regs.get(reg,0)&0xffff)|(val<<16)
                s=path_at(regs[reg])
                if s:line["public_path"]=s
            if i.mnemonic in ("bl","blx") and re.fullmatch(r"#0x[0-9a-f]+",i.op_str):
                line["call_candidate"]=syms.get(int(i.op_str[1:],16),("",0))[0]
            lines.append(line)
        rows[name]=lines
    if set(rows)!={"main","hash_file"}:raise ValueError("incomplete function symbols")
    paths=sorted({x.decode() for x in re.findall(rb"/(?:var|mnt|dev|etc|proc|tmp|eso|run)/[A-Za-z0-9_./%+-]{2,150}",raw)})
    report={"public_paths":paths,"functions":rows,"runtime_verified":False,"installable":False}
    Path(sys.argv[2]).write_text(json.dumps(report,indent=2)+"\n")
    print("LAUNCHER_PREREQUISITE_PATHS",json.dumps(paths))
    for name,lines in rows.items():
        print("LAUNCHER_FUNCTION",name)
        for line in lines:print(json.dumps(line))
