#!/usr/bin/env python3
"""Bounded constant propagation on pinned original SI, including GOT-relative
address arithmetic. Only allowlisted executable names are emitted."""
import io,json,sys,struct,subprocess,re
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM,CS_GRP_CALL,CS_GRP_JUMP,CS_GRP_RET
from capstone.arm import ARM_OP_REG,ARM_OP_IMM,ARM_OP_MEM,ARM_REG_PC,ARM_CC_AL
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original,sha,imm
from launch_provider import scan
MASK=0xffffffff
def analyze(data):
    e=ELFFile(io.BytesIO(data));s=e.get_section_by_name(".text")
    start=int(s["sh_addr"]);code=s.data();end=start+len(code)
    seg=[(int(p["p_vaddr"]),p.data()) for p in e.iter_segments() if p["p_type"]=="PT_LOAD"]
    def word(a):
        for v,d in seg:
            if v<=a and a+4<=v+len(d):return struct.unpack_from("<I",d,a-v)[0]
        return None
    names={}
    for v,d in seg:
        for m in re.finditer(rb"(?<![A-Za-z0-9_])(?:/[A-Za-z0-9_.-]+)*/?dio_manager\x00",d):
            names[v+m.start()]=m.group()[:-1].decode()
            names[v+m.end()-12]="dio_manager"
    known=scan(data)
    names.update({int(k,16):v for k,v in known["dio_strings"].items()})
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
    # Map all imported call targets from the exact ELF, not inferred names.
    rel={}
    for t in e.iter_sections():
        if isinstance(t,RelocationSection):
            tab=e.get_section(t["sh_link"])
            for r in t.iter_relocations():
                if r["r_info_type"]==22 and r["r_info_sym"]:
                    rel[int(r["r_offset"])]=tab.get_symbol(r["r_info_sym"]).name
    plt=e.get_section_by_name(".plt");d=plt.data();v=int(plt["sh_addr"]);imports={}
    for o in range(0,len(d)-11,4):
        a,b,c=struct.unpack_from("<III",d,o)
        if a&0xfffff000==0xe28fc000 and b&0xfffff000==0xe28cc000 and c&0xfffff000==0xe5bcf000:
            name=rel.get((v+o+8+imm(a)+imm(b)+(c&4095))&MASK)
            if name:imports[v+o]=name
    raw=list(imports.values())
    dem=subprocess.run(["c++filt"],input="\n".join(raw)+"\n",text=True,capture_output=True,check=True).stdout.splitlines()
    imports=dict(zip(imports,dem))
    # Every PC-relative literal load is a seed. Track up to 96 instructions
    # in straight line only; calls invalidate caller-saved registers.
    # Stop at branches/conditional instructions; never claim runtime proof.
    refs=set();matches=[];call_matches=[]
    def reg(r,pc,env):return (pc+8)&MASK if r==ARM_REG_PC else env.get(r)
    def operand(o,pc,env):
        if o.shift.type:return None
        if o.type==ARM_OP_IMM:return o.imm&MASK
        if o.type==ARM_OP_REG:return reg(o.reg,pc,env)
        return None
    for off in range(0,len(code)-3,4):
        w=struct.unpack_from("<I",code,off)[0]
        if w&0x0f7f0000!=0x051f0000 or w>>28!=14:continue
        seed=start+off;env={}
        for i in md.disasm(code[off:off+384],seed):
            if i.cc!=ARM_CC_AL or i.group(CS_GRP_JUMP) or i.group(CS_GRP_RET):break
            ops=i.operands;pc=i.address;value=None;dst=None
            if i.group(CS_GRP_CALL):
                dest=operand(ops[-1],pc,env) if ops else None
                for r in list(env):
                    if md.reg_name(r) in ("r0","r1","r2","r3"):
                        val=env[r]
                        if val in names:
                            key=(pc,r,val)
                            if key not in refs:
                                refs.add(key);call_matches.append({"site":hex(pc),"register":md.reg_name(r),"name":names[val],"target":imports.get(dest,hex(dest) if dest is not None else "indirect")})
                for r in list(env):
                    if md.reg_name(r) in ("r0","r1","r2","r3","ip","r12","lr"):env.pop(r,None)
                continue
            if ops and ops[0].type==ARM_OP_REG:dst=ops[0].reg
            if dst is not None and not i.writeback:
                if i.mnemonic=="ldr" and len(ops)==2 and ops[1].type==ARM_OP_MEM:
                    m=ops[1].mem;a=reg(m.base,pc,env);idx=reg(m.index,pc,env) if m.index else 0
                    if a is not None and idx is not None and not ops[1].shift.type:
                        value=word((a+m.disp+idx*m.scale)&MASK)
                elif i.mnemonic=="mov" and len(ops)==2:value=operand(ops[1],pc,env)
                elif i.mnemonic in ("add","sub") and len(ops)==3:
                    a=operand(ops[1],pc,env);b=operand(ops[2],pc,env)
                    if a is not None and b is not None:value=(a+b if i.mnemonic=="add" else a-b)&MASK
            try:_,writes=i.regs_access()
            except Exception:break
            if ARM_REG_PC in writes:break
            for r in writes:env.pop(r,None)
            if dst in writes and value is not None:
                env[dst]=value
                if value in names:
                    key=(pc,dst,value)
                    if key not in refs:
                        refs.add(key);matches.append({"seed":hex(seed),"site":hex(pc),"register":md.reg_name(dst),"name":names[value]})
            if not env:break
    return {"sha256":sha(data),"name_addresses":{hex(k):v for k,v in names.items()},"computed_refs":matches,"call_argument_candidates":call_matches,"runtime_verified":False,"installable":False}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    r=analyze(get_original(root,"smartphone_integrator"))
    out.write_text(json.dumps(r,indent=2)+"\n")
    print("R105288_SI_DATAFLOW",json.dumps(r)[:18000])
