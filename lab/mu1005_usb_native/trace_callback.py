#!/usr/bin/env python3
"""Follow the actual R105.276 USB-removal callback and SI reset.
Read exact original ARM executable; no patch or inferred session authority."""
import io,json,struct,sys
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_REG_PC
from elftools.elf.elffile import ELFFile
from native_map import get_original, sha
def analyze(data):
    e=ELFFile(io.BytesIO(data))
    s=e.get_section_by_name(".text")
    start=int(s["sh_addr"]);code=s.data();end=start+len(code)
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
    def window(a,n):
        if not start<=a<end:return []
        return [{"va":hex(i.address),"op":i.mnemonic,"args":i.op_str}
            for i in md.disasm(code[a-start:min(a-start+n,len(code))],a)]
    def word(a):
        for p in e.iter_segments():
            v=int(p["p_vaddr"]);n=int(p["p_filesz"])
            if p["p_type"]=="PT_LOAD" and v<=a and a+4<=v+n:
                return struct.unpack_from("<I",data,int(p["p_offset"])+a-v)[0]
        return None
    def edges(a,n):
        out=[]
        for i in md.disasm(code[a-start:min(a-start+n,len(code))],a):
            if i.mnemonic in ("bl","blx") and i.operands and i.operands[-1].type==ARM_OP_IMM:
                out.append({"site":hex(i.address),"target":hex(i.operands[-1].imm&0xffffffff)})
            elif i.mnemonic.startswith("ldr") and len(i.operands)>1 and i.operands[1].type==ARM_OP_MEM and i.operands[1].mem.base==ARM_REG_PC:
                slot=i.address+8+i.operands[1].mem.disp
                v=word(slot)
                out.append({"site":hex(i.address),"literal":hex(slot),"value":hex(v) if v is not None else None})
        return out
    def refs(target):
        out=[]
        for o in range(0,len(code)-3,4):
            w=struct.unpack_from("<I",code,o)[0]
            if w&0x0e000000!=0x0a000000 or w>>28==15:continue
            rel=(w&0xffffff)<<2
            if rel&0x2000000:rel-=0x4000000
            a=start+o
            if (a+8+rel)&0xffffffff==target:
                out.append({"site":hex(a),"word":hex(w),"context":window(max(start,a-32),64)})
        return out
    # Registered callbacks are function pointers, not necessarily BL targets.
    def pointer_refs(target):
        out=[]
        for p in e.iter_segments():
            if p["p_type"]!="PT_LOAD":continue
            off=int(p["p_offset"]);n=int(p["p_filesz"]);va=int(p["p_vaddr"])
            for i in range(0,n-3,4):
                if struct.unpack_from("<I",data,off+i)[0]==target:
                    a=va+i
                    out.append({"address":hex(a),"context":window(max(start,a-24),48) if start<=a<end else [],"segment_flags":int(p["p_flags"])})
        return out
    callbacks=[0x15d4d0,0x15d4e4,0x15d4f8]
    rows=[]
    for a in callbacks:
        assert start<=a<end
        rows.append({"entry":hex(a),"instructions":window(a,20 if a!=callbacks[-1] else 64),
          "edges":edges(a,20 if a!=callbacks[-1] else 64),"direct_refs":refs(a),"pointer_refs":pointer_refs(a)})
    # Resolve immediate unconditional tail branches from the small thunks.
    tails=set()
    for a in callbacks:
        for i in md.disasm(code[a-start:a-start+20],a):
            if i.mnemonic=="b" and i.operands and i.operands[0].type==ARM_OP_IMM:
                t=i.operands[0].imm&0xffffffff
                if start<=t<end:tails.add(t)
    reset=0x188018
    assert start<=reset<end
    return {"sha256":sha(data),"callbacks":rows,
      "tail_targets":[{"entry":hex(a),"instructions":window(a,160),"edges":edges(a,160),"refs":refs(a)} for a in sorted(tails)],
      "reset":{"entry":hex(reset),"context":window(reset-96,180),"edges":edges(reset-96,180),"direct_refs":refs(reset)}}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    report=analyze(get_original(root,"smartphone_integrator"))
    out.write_text(json.dumps({"scope":"EXACT_USB_CALLBACK_AND_RESET_EDGES","runtime_verified":False,"installable":False,"result":report},indent=2)+"\n")
    for r in report["callbacks"]:
        print("USB_CALLBACK",r["entry"],json.dumps(r["instructions"]), "POINTER_REFS",json.dumps(r["pointer_refs"][:12]))
    for r in report["tail_targets"]:print("USB_TAIL",r["entry"],json.dumps(r["instructions"]),json.dumps(r["edges"]))
    print("SI_RESET",json.dumps(report["reset"]["context"]))
