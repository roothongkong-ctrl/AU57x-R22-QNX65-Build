#!/usr/bin/env python3
"""Resolve PC-relative DIO string producers and the stock ProcessTNG owner.
Read-only, pinned MU1005. No runtime hook or authorization inference.
"""
import io, json, struct, sys
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_GRP_JUMP, CS_GRP_CALL, CS_GRP_RET
from capstone.arm import ARM_OP_REG, ARM_OP_MEM, ARM_OP_IMM, ARM_REG_PC, ARM_CC_AL
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from launch_provider import scan
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original, sha

def analyze(data):
    prior=scan(data)
    e=ELFFile(io.BytesIO(data))
    sec=e.get_section_by_name(".text")
    start=int(sec["sh_addr"]); code=sec.data(); end=start+len(code)
    loads=[p for p in e.iter_segments() if p["p_type"]=="PT_LOAD"]
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM); md.detail=True
    strings={int(k,16):v for k,v in prior["dio_strings"].items()}
    def word(a):
        for p in loads:
            v=int(p["p_vaddr"])
            if v<=a and a+4<=v+int(p["p_filesz"]):
                return struct.unpack_from("<I",data,int(p["p_offset"])+a-v)[0]
        return None
    def decode(a,n):
        if not start<=a<end:return []
        return list(md.disasm(code[a-start:min(a-start+n,len(code))],a))
    def record(i):
        return {"va":hex(i.address),"word":i.bytes.hex(),"op":i.mnemonic,"args":i.op_str}
    def window(a,n):
        return [record(i) for i in decode(a,n)]
    def edges(a,n):
        out=[]
        for i in decode(a,n):
            if i.group(CS_GRP_CALL) and i.operands and i.operands[-1].type==ARM_OP_IMM:
                out.append({"site":hex(i.address),"target":hex(i.operands[-1].imm&0xffffffff)})
        return out
    funcs=[]
    for tab in e.iter_sections():
        if isinstance(tab,SymbolTableSection):
            for s in tab.iter_symbols():
                a=int(s["st_value"]);n=int(s["st_size"])
                if s["st_info"]["type"]=="STT_FUNC" and n and start<=a<end and a+n<=end:
                    funcs.append((a,n,s.name))
    def owners(a):
        return [{"entry":hex(b),"size":n,"symbol":name} for b,n,name in funcs if b<=a<b+n]
    # Scan literal loads, then follow only a straight-line, unconditional
    # LDR -> ADD/SUB PC sequence. Stop on any write to the source register,
    # control transfer or conditional instruction. A matched arithmetic
    # reference proves an address calculation, not the runtime owner.
    pic=[]
    for off in range(0,len(code)-3,4):
        w=struct.unpack_from("<I",code,off)[0]
        if w&0x0f7f0000!=0x051f0000 or w>>28!=14:continue
        a=start+off; seq=decode(a,48)
        if not seq:continue
        i=seq[0]; ops=i.operands
        if len(ops)!=2 or ops[0].type!=ARM_OP_REG or ops[1].type!=ARM_OP_MEM or ops[1].mem.base!=ARM_REG_PC:continue
        r=ops[0].reg
        if r==ARM_REG_PC:continue
        slot=a+8+ops[1].mem.disp; v=word(slot)
        if v is None:continue
        for j in seq[1:]:
            if j.cc!=ARM_CC_AL or j.group(CS_GRP_JUMP) or j.group(CS_GRP_CALL) or j.group(CS_GRP_RET):break
            o=j.operands
            if j.mnemonic in ("add","sub") and len(o)==3 and all(x.type==ARM_OP_REG for x in o) and not any(x.shift.type for x in o):
                x,y=o[1].reg,o[2].reg
                result=None
                if x==ARM_REG_PC and y==r:
                    result=(j.address+8+v if j.mnemonic=="add" else j.address+8-v)&0xffffffff
                elif j.mnemonic=="add" and x==r and y==ARM_REG_PC:
                    result=(j.address+8+v)&0xffffffff
                if result in strings:
                    pic.append({"load":record(i),"arithmetic":record(j),"literal_slot":hex(slot),"literal_value":hex(v),"target":hex(result),"name":strings[result],"owner_symbols":owners(j.address),"context":window(max(start,a-24),96)})
            try: _,writes=j.regs_access()
            except Exception:break
            if r in writes or ARM_REG_PC in writes:break
    anchors=[]
    for row in prior["calls"]:
        a=int(row["site"],16)
        # report only a bounded window around each known call and real symbols
        # when present; a nearby push is not a proven function entry.
        nearby=[]
        for i in decode(max(start,a-320),320):
            if i.mnemonic=="push" and "lr" in i.op_str:
                nearby.append(hex(i.address))
        entries=owners(a)
        targets={int(x["entry"],16) for x in entries}
        callers=[]
        if targets:
            for off in range(0,len(code)-3,4):
                w=struct.unpack_from("<I",code,off)[0]
                if w&0x0f000000!=0x0b000000 or w>>28==15:continue
                rel=(w&0xffffff)<<2
                if rel&0x2000000:rel-=0x4000000
                b=start+off;dst=(b+8+rel)&0xffffffff
                if dst in targets:callers.append({"site":hex(b),"target":hex(dst),"guard":hex(w),"owner_symbols":owners(b)})
        anchors.append({"site":row["site"],"symbol":row["symbol"],"owners":entries,"nearby_prologue_candidates":nearby,"direct_owner_callers":callers,"context":window(max(start,a-160),240),"call_edges":edges(max(start,a-160),240)})
    return {"sha256":sha(data),"dio_strings":prior["dio_strings"],"direct_refs":prior["direct_literal_refs"],"pic_refs":pic,"anchors":anchors,"runtime_verified":False,"caller_proven":False,"installable":False}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    r=analyze(get_original(root,"smartphone_integrator"))
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(r,indent=2)+"\n")
    print("SI_DIO_PIC_REFS",json.dumps(r["pic_refs"]))
    for a in r["anchors"]:
        print("SI_PROCESS_OWNER",json.dumps({k:v for k,v in a.items() if k!="context"}))
