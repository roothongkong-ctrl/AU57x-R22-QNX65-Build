#!/usr/bin/env python3
"""Identify unmodified upstream launcher's normal error returns.
Read-only: no patch, fake identity, or authentication/license bypass."""
import hashlib,json,sys,re
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
PINS={"AID10.5-17PM":"41d523d4f2456237e6ed99bd89a7a8777aff5c2641c2f1663df697934f821813","AID12.3":"07955ac794986416fa30122a5964eb8de35f3b93955b1d4d2cb7bc51f7dc3140"}
def inspect(p,profile):
    raw=p.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=PINS[profile]:raise ValueError("unexpected launcher identity")
    with p.open("rb") as f:
        e=ELFFile(f);names={};func=[]
        for s in e.iter_sections():
            if isinstance(s,SymbolTableSection):
                for x in s.iter_symbols():
                    if x["st_info"]["type"]=="STT_FUNC" and x["st_shndx"]!="SHN_UNDEF" and x["st_value"]:
                        names[int(x["st_value"])&~1]=x.name
                        if x["st_size"]:func.append((int(x["st_value"])&~1,int(x["st_size"]),x.name))
        rel=e.get_section_by_name(".rel.plt");plt=e.get_section_by_name(".plt")
        if rel and plt:
            table=e.get_section(rel["sh_link"])
            for i,x in enumerate(rel.iter_relocations()):
                # ARM GNU PLT0 20 bytes then 12-byte entries; mark candidate only.
                names[int(plt["sh_addr"])+20+12*i]="PLT_CANDIDATE:"+table.get_symbol(x["r_info_sym"]).name
        md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
        s=e.get_section_by_name(".text")
        if not s:raise ValueError("missing .text")
        ins=list(md.disasm(s.data(),int(s["sh_addr"])))
        def row(i):
            target=None
            if i.mnemonic.startswith("b") and re.fullmatch(r"#0x[0-9a-f]+",i.op_str):
                a=int(i.op_str[1:],16);target=names.get(a)
            return {"addr":hex(i.address),"op":i.mnemonic,"args":i.op_str,"target_candidate":target}
        matches=[]
        for j,i in enumerate(ins):
            if not re.search(r"(?:^|,\s*)#(?:0xb|11)$",i.op_str):continue
            if not i.mnemonic.startswith(("mov","cmp","add","sub","str")):continue
            owners=[n for a,z,n in func if a<=i.address<a+z]
            matches.append({"address":hex(i.address),"function":owners,"context":[row(k) for k in ins[max(0,j-10):j+12]]})
        paths=sorted({v.decode() for v in re.findall(rb"/(?:mnt|dev|etc|proc|tmp|eso)/[A-Za-z0-9_./-]{2,160}",raw)})
        # Limit literals to filesystem paths; no identity/credential contents.
        return {"profile":profile,"sha256":PINS[profile],"exit11_immediate_candidates":matches,
                "public_path_literals":paths,"scope":"static ARM disassembly candidates, not a verified exit branch or permission to change checks"}
def main():
    root=Path(sys.argv[1])/"standalone/SD_ROOT/carplay/payload/wireless/profiles"
    rows=[inspect(root/p/"wireless-receiver-exec",p) for p in PINS]
    for x in rows:print("LAUNCHER_EXIT11_AUDIT",json.dumps(x))
    Path(sys.argv[2]).write_text(json.dumps({"rows":rows,"runtime_verified":False,"installable":False},indent=2)+"\n")
if __name__=="__main__":main()
