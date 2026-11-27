#!/usr/bin/env python3
"""Bounded ARM callsite evidence for R105.141 exact stock Bluetooth reference."""
import bisect,hashlib,io,json,struct,sys
from pathlib import Path
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from elftools.elf.relocation import RelocationSection
PIN="7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272"
ANCHOR=0x1562f4
def decode(code,base,names):
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
    calls=[];branches=[]
    for i in md.disasm(code,base):
        if not i.id:continue
        if not i.operands or i.operands[-1].type!=ARM_OP_IMM:continue
        dest=i.operands[-1].imm & 0xffffffff
        if i.mnemonic in ("bl","blx") or (i.mnemonic.startswith("bl") and i.mnemonic[2:] in ("eq","ne","gt","lt","ge","le","hi","ls","cc","cs","mi","pl","vs","vc")):
            calls.append({"va":hex(i.address),"target":hex(dest),"symbols":names.get(dest,[])})
        elif i.mnemonic in ("b","beq","bne","bgt","blt","bge","ble","bhi","bls","bcc","bcs","bmi","bpl"):
            branches.append({"va":hex(i.address),"kind":i.mnemonic,"target":hex(dest)})
    return calls,branches
def selftest():
    c,b=decode(struct.pack("<II",0xeb000002,0x1a000000),0x1000,{0x1010:["example"]})
    assert c==[{"va":"0x1000","target":"0x1010","symbols":["example"]}],c
    assert b==[{"va":"0x1004","kind":"bne","target":"0x100c"}],b
    c,_=decode(struct.pack("<II",0xffffffff,0xeb000002),0x1000,{})
    assert c and c[-1]["va"]=="0x1004",c
    print("IAP_CALLCHAIN_SELFTEST_OK")
def main(root,out):
    data=None
    for p in root.rglob("bluetooth"):
        if p.is_file() and p.resolve().is_relative_to(root):
            d=p.read_bytes()
            if hashlib.sha256(d).hexdigest()==PIN:data=d;break
    if data is None:raise SystemExit("PINNED_BLUETOOTH_MISSING")
    e=ELFFile(io.BytesIO(data));assert e.elfclass==32 and e.little_endian and e["e_machine"]=="EM_ARM"
    text=e.get_section_by_name(".text");assert text is not None
    va=int(text["sh_addr"]);code=text.data();assert va<=ANCHOR<va+len(code)
    names={};funcs=[]
    for sec in e.iter_sections():
        if not isinstance(sec,SymbolTableSection):continue
        for s in sec.iter_symbols():
            v=int(s["st_value"]);size=int(s["st_size"])
            if not s.name or not v or s["st_shndx"]=="SHN_UNDEF":continue
            if s["st_info"]["type"]=="STT_FUNC":
                names.setdefault(v,[]).append(s.name)
                if size and va<=v<va+len(code):funcs.append((v,size,s.name))
    # ARM PLT layout is not assumed: decode each standard 3-instruction stub
    # and resolve its computed GOT slot against the actual JUMP_SLOT relocations.
    got={}
    for sec in e.iter_sections():
        if isinstance(sec,RelocationSection):
            sym=e.get_section(sec["sh_link"])
            for rel in sec.iter_relocations():
                if rel["r_info_type"]==22 and rel["r_info_sym"]:
                    got[int(rel["r_offset"])]=sym.get_symbol(rel["r_info_sym"]).name
    plt=e.get_section_by_name(".plt")
    if plt is not None:
        pva=int(plt["sh_addr"]);pd=plt.data()
        def imm(w):
            n=w&255;shift=((w>>8)&15)*2
            return ((n>>shift)|(n<<(32-shift)))&0xffffffff if shift else n
        for o in range(0,len(pd)-11,4):
            a,b,c=struct.unpack_from("<III",pd,o)
            if (a&0xfffff000)!=0xe28fc000 or (b&0xfffff000)!=0xe28cc000:continue
            if (c&0xfffff000)!=0xe5bcf000:continue
            slot=(pva+o+8+imm(a)+imm(b)+(c&4095))&0xffffffff
            if slot in got:names.setdefault(pva+o,[]).append(got[slot]+"@plt")
    names={k:sorted(set(v)) for k,v in names.items()}
    containing=sorted(set((v,n,s) for v,n,s in funcs if v<=ANCHOR<v+n),key=lambda x:x[1])
    if containing and containing[0][1]<=16384:
        start,size,_=containing[0];end=min(start+size,va+len(code));boundary="symbol"
    else:
        start=max(va,ANCHOR-128);end=min(va+len(code),ANCHOR+384);boundary="bounded_window_not_function"
    # ARM EHABI PREL31 unwind entries can bound a stripped function.
    if boundary!="symbol":
        ex=e.get_section_by_name(".ARM.exidx")
        if ex is not None:
            a=int(ex["sh_addr"]);d=ex.data();bounds=[]
            for off in range(0,len(d)-7,8):
                word=struct.unpack_from("<I",d,off)[0]&0x7fffffff
                if word&0x40000000:word-=0x80000000
                f=(a+off+word)&0xffffffff
                if va<=f<va+len(code):bounds.append(f)
            bounds=sorted(set(bounds));k=bisect.bisect_right(bounds,ANCHOR)
            if 0<k<len(bounds) and bounds[k]-bounds[k-1]<=16384:
                start,end=bounds[k-1],bounds[k];boundary="exidx_candidate"
    start=(start+3)&~3;end&=~3
    calls,branches=decode(code[start-va:end-va],start,names)
    immediate=[c for c in calls if ANCHOR<=int(c["va"],16)<=ANCHOR+96]
    upstream=[]
    if boundary in ("symbol","exidx_candidate"):
        md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
        for off in range(0,len(code)-3,4):
            w=struct.unpack_from("<I",code,off)[0]
            if (w&0x0f000000)!=0x0b000000:continue
            rel=(w&0xffffff)<<2
            if rel&0x2000000:rel-=0x4000000
            if (va+off+8+rel)&0xffffffff!=start:continue
            v=va+off
            owners=sorted((n,s) for a,n,s in funcs if a<=v<a+n)
            upstream.append({"va":hex(v),"owner":owners[0][1] if owners else None})
    report={"scope":"STATIC_DIRECT_CALL_CANDIDATES","anchor":hex(ANCHOR),"sha256":PIN,
      "boundary":boundary,"start":hex(start),"end":hex(end),
      "containing_symbols":[{"va":hex(v),"size":n,"symbol":s} for v,n,s in containing[:10]],
      "nearby_calls":immediate,"calls":calls[:120],"conditional_branches":branches[:120],
      "direct_callers":upstream[:120],"mapped_plt_entries":sum(any(s.endswith("@plt") for s in v) for v in names.values()),
      "wireless_verified":False,"installable":False}
    out.write_text(json.dumps(report,indent=2)+"\n");print(json.dumps(report,sort_keys=True))
if __name__=="__main__":
    if sys.argv[1:]==["--selftest"]:selftest()
    else:main(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]))
