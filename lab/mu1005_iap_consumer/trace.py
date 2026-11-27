#!/usr/bin/env python3
"""Find direct ARM/PIC references to the actual stock Bluetooth iAP controls.
Static candidates only: never modify original bytes or infer registration success.
"""
import hashlib,io,json,struct,sys
from pathlib import Path
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_REG,ARM_OP_IMM,ARM_OP_MEM,ARM_REG_PC,ARM_CC_AL,ARM_CC_INVALID
from elftools.elf.elffile import ELFFile
PIN="7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272"
MARKERS=("bluetooth.enableIap","iapEnabled","SERVICETYPE_IAP2")
MASK=0xffffffff
def trace(code,base,word,targets):
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
    values={};hits=[];last=None
    for ins in md.disasm(code,base):
        if last is not None and ins.address!=last:values.clear()
        last=ins.address+ins.size
        if not ins.id:
            values.clear();continue
        # Restrict to unconditional, unshifted operands and bounded linear windows.
        values={r:(v,at) for r,(v,at) in values.items() if ins.address-at<=48}
        op=ins.operands;m=ins.mnemonic
        if m in ("b","bx","bl","blx","pop") or ins.cc not in (ARM_CC_AL,ARM_CC_INVALID):
            values.clear();continue
        def val(o):
            if o.shift.type!=0:return None
            if o.type==ARM_OP_IMM:return o.imm&MASK
            if o.type==ARM_OP_REG:
                if o.reg==ARM_REG_PC:return (ins.address+8)&MASK
                return values.get(o.reg,(None,0))[0]
            return None
        dest=op[0].reg if op and op[0].type==ARM_OP_REG else None
        result=None
        if dest and dest!=ARM_REG_PC:
            if m in ("mov","movw") and len(op)==2:result=val(op[1])
            elif m=="movt" and len(op)==2:
                a=values.get(dest,(None,0))[0];b=val(op[1])
                if a is not None and b is not None:result=(a&0xffff)|(b<<16)
            elif m in ("add","sub") and len(op)==3:
                a,b=val(op[1]),val(op[2])
                if a is not None and b is not None:result=a+b if m=="add" else a-b
            elif m=="ldr" and len(op)==2 and op[1].type==ARM_OP_MEM and not ins.writeback:
                mem=op[1].mem
                a=ins.address+8 if mem.base==ARM_REG_PC else values.get(mem.base,(None,0))[0]
                if a is not None and mem.index==0:result=word((a+mem.disp)&MASK)
            elif m=="adr" and len(op)==2 and op[1].type==ARM_OP_IMM:
                result=val(op[1])
        try:_,writes=ins.regs_access()
        except Exception:
            values.clear();continue
        for r in writes:values.pop(r,None)
        if ARM_REG_PC in writes:
            values.clear();continue
        if result is not None:
            result &= MASK
            values[dest]=(result,ins.address)
            if result in targets:
                hits.append({"marker":targets[result],"va":hex(ins.address),"mode":m,"target_va":hex(result)})
    return hits
def selftest():
    code=struct.pack("<III",0xe59f0000,0xe08f0000,0x20)
    def word(a):return 0x20 if a==0x1008 else None
    hits=trace(code,0x1000,word,{0x102c:"synthetic"})
    assert any(h["va"]=="0x1004" and h["marker"]=="synthetic" for h in hits),hits
    assert not trace(code,0x1000,word,{0x5555:"other"})
    print("ARM_PIC_REFERENCE_SELFTEST_OK")
def main(root,out):
    files={}
    for p in root.rglob("bluetooth"):
        if p.is_file() and p.resolve().is_relative_to(root):
            b=p.read_bytes()
            if hashlib.sha256(b).hexdigest()==PIN:files[PIN]=b
    if PIN not in files:raise SystemExit("PINNED_BLUETOOTH_MISSING")
    data=files[PIN];elf=ELFFile(io.BytesIO(data))
    assert elf.elfclass==32 and elf.little_endian and elf["e_machine"]=="EM_ARM"
    loads=[(int(s["p_offset"]),int(s["p_vaddr"]),int(s["p_filesz"])) for s in elf.iter_segments() if s["p_type"]=="PT_LOAD"]
    def word(a):
        for off,va,n in loads:
            if va<=a and a+4<=va+n:return struct.unpack_from("<I",data,off+a-va)[0]
        return None
    targets={}
    for marker in MARKERS:
        needle=marker.encode()+b"\0";start=0
        while (pos:=data.find(needle,start))!=-1:
            start=pos+1
            for off,va,n in loads:
                if off<=pos and pos+len(needle)<=off+n:targets[va+pos-off]=marker
    if set(targets.values())!=set(MARKERS):raise SystemExit("PINNED_MARKER_MAPPING_INCOMPLETE")
    text=elf.get_section_by_name(".text")
    if text is None:raise SystemExit("TEXT_SECTION_MISSING")
    hits=trace(text.data(),int(text["sh_addr"]),word,targets)
    report={"scope":"ARM_LINEAR_DIRECT_REFERENCE_CANDIDATES_ONLY","sha256":PIN,
      "markers":[{"name":m,"va":hex(a)} for a,m in sorted(targets.items())],
      "references":hits[:200],"total_references":len(hits),
      "not_observed":sorted(set(MARKERS)-{h["marker"] for h in hits}),
      "limitations":["ARM only","bounded constant propagation","no indirect control flow","no runtime owner proof"],
      "wireless_verified":False,"installable":False}
    out.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,sort_keys=True))
if __name__=="__main__":
    if sys.argv[1:]==["--selftest"]:selftest()
    else:main(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]))
