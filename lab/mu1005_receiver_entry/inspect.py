#!/usr/bin/env python3
"""Find actual v75gen provider and map observed QNX fault without patching."""
import hashlib,json,re,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from elftools.elf.relocation import RelocationSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
PIN={"AID10.5-17PM":"396c73bcd7674483245776d0493552832f7ebbcefed399d7b9dd1182a5336c90","AID12.3":"f195665b0e9fb8a592618e64b7201ddfc536a9b082c6dd35db214b99d5e01de1"}
FAULT=0x0103f988;OFFSET=0x3f988
def examine(p,root,receiver=False):
    with p.open("rb") as f:
        if f.read(4)!=b"\x7fELF":return None
        f.seek(0);e=ELFFile(f)
        if e["e_machine"]!="EM_ARM" or e.elfclass!=32 or not e.little_endian:return None
        syms=[];relocs=[];paths=[];maps=[]
        for s in e.iter_sections():
            if isinstance(s,SymbolTableSection):
                for x in s.iter_symbols():
                    if x.name=="v75gen":syms.append({"table":s.name,"index":str(x["st_shndx"]),"binding":x["st_info"]["bind"],"type":x["st_info"]["type"],"value":hex(x["st_value"]),"size":x["st_size"]})
            if isinstance(s,RelocationSection):
                t=e.get_section(s["sh_link"])
                if not isinstance(t,SymbolTableSection):continue
                for x in s.iter_relocations():
                    if x["r_info_sym"] and t.get_symbol(x["r_info_sym"]).name=="v75gen":
                        relocs.append({"section":s.name,"offset":hex(x["r_offset"]),"type":x["r_info_type"]})
        if receiver:
            loads=[x for x in e.iter_segments() if x["p_type"]=="PT_LOAD" and x["p_flags"]&1]
            md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
            for seg in loads:
                v=int(seg["p_vaddr"]);o=int(seg["p_offset"]);d=seg.data()
                # QNX reported mapaddr can mean relative to mapped segment.
                # Report interpretations separately, not a fabricated exact PC.
                cand={"reported_ip_as_va":FAULT,"mapaddr_as_va":OFFSET,"mapaddr_from_segment_base":v+OFFSET,"mapaddr_as_file_offset":v+OFFSET-o}
                for method,a in cand.items():
                    if v<=a<v+len(d) and a%4==0:
                        lo=max(v,a-32);hi=min(v+len(d),a+36)
                        maps.append({"method":method,"address":hex(a),"segment_va":hex(v),"instructions":[{"address":hex(i.address),"op":i.mnemonic,"args":i.op_str,"fault_candidate":i.address==a} for i in md.disasm(d[lo-v:hi-v],lo)]})
        if p.name in ("wireless-receiver-exec","wireless-entry"):
            # Public launcher path literals only, not key or auth contents.
            f.seek(0);raw=f.read()
            paths=sorted({m.decode() for m in re.findall(rb"/(?:mnt/app/carplaymaps|dev/shmem|mnt/persist)/[A-Za-z0-9_.-]+",raw)})
        return {"file":str(p.relative_to(root)),"sha256":hashlib.sha256(p.read_bytes()).hexdigest(),"type":e["e_type"],"entry":hex(e["e_entry"]),"v75gen":syms,"relocations":relocs,"launcher_paths":paths,"fault_candidates":maps}
def main(root,out):
    root=root.resolve();prefix=root/"standalone/SD_ROOT/carplay/payload"
    result=[];receivers=[]
    for p in sorted(prefix.rglob("*")):
        if p.is_symlink() or not p.is_file():continue
        is_receiver=p.name=="wcp_receiver"
        r=examine(p,root,is_receiver)
        if r is None:continue
        if is_receiver:
            if p.parent.name not in PIN or PIN[p.parent.name]!=r["sha256"]:raise ValueError("receiver hash mismatch")
            receivers.append(r)
        if r["v75gen"] or r["launcher_paths"] or is_receiver:result.append(r)
    if len(receivers)!=2:raise ValueError("missing receiver profiles")
    providers=[r["file"] for r in result if any(s["index"]!="SHN_UNDEF" and s["table"]==".dynsym" and s["binding"] in ("STB_GLOBAL","STB_WEAK") for s in r["v75gen"])]
    for r in result:
        print("DONOR_COMPONENT",json.dumps(r))
    print("V75GEN_ACTUAL_EXPORT_CANDIDATES",json.dumps(providers))
    out.write_text(json.dumps({"providers":providers,"components":result,"runtime_verified":False,"installable":False},indent=2)+"\n")
if __name__=="__main__":main(Path(sys.argv[1]),Path(sys.argv[2]))
