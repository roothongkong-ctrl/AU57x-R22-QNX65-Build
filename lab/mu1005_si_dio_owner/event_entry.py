#!/usr/bin/env python3
"""Trace actual R106 SI log events to pinned original code, not guessed DIO names.
Report only static address candidates; no binary patch or OEM runtime claim."""
import io,json,re,struct,sys,bisect
from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM,CS_GRP_CALL,CS_GRP_JUMP,CS_GRP_RET
from capstone.arm import ARM_OP_REG,ARM_OP_IMM,ARM_OP_MEM,ARM_REG_PC
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original,sha
from launch_provider import scan
KEYS=(b"connectDevice:",b"updateDeviceConnectionState:",b"disableBluetooth")
MASK=0xffffffff
def trace(data,name):
    e=ELFFile(io.BytesIO(data));sec=e.get_section_by_name(".text")
    start=int(sec["sh_addr"]);code=sec.data();end=start+len(code)
    seg=[(int(p["p_vaddr"]),p.data()) for p in e.iter_segments() if p["p_type"]=="PT_LOAD"]
    def word(a):
        for v,d in seg:
            if v<=a and a+4<=v+len(d):return struct.unpack_from("<I",d,a-v)[0]
        return None
    strings={}
    for v,d in seg:
        for k in KEYS:
            for m in re.finditer(re.escape(k),d):
                # C string start may include a logging prefix; no raw string emitted.
                n=m.start();lo=n
                while lo>0 and n-lo<160 and 32<=d[lo-1]<127:lo-=1
                if d.find(b"\0",n,min(n+256,len(d)))<0:continue
                strings[v+lo]=k.decode();strings[v+n]=k.decode()
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
    refs={};calls={};entries=set()
    # ARM EHABI prel31 table: valid text targets only, no claim every
    # function has an entry or that gaps identify a function uniquely.
    for x in e.iter_sections():
        if not x.name.startswith(".ARM.exidx"):continue
        d=x.data();v=int(x["sh_addr"])
        for off in range(0,len(d)-7,8):
            a=struct.unpack_from("<I",d,off)[0]&0x7fffffff
            if a&0x40000000:a-=0x80000000
            a=(v+off+a)&MASK
            if start<=a<end and a%4==0:entries.add(a)
    entries=sorted(entries)
    def owner(a):
        j=bisect.bisect_right(entries,a)-1
        return entries[j] if j>=0 else None
    def ref(a,val,method):
        if val in strings:refs[(a,val)]={"site":hex(a),"string":strings[val],"target":hex(val),"method":method}
    for off in range(0,len(code)-3,4):
        a=start+off;w=struct.unpack_from("<I",code,off)[0]
        if w&0x0f000000==0x0b000000 and w>>28!=15:
            rel=(w&0xffffff)<<2
            if rel&0x2000000:rel-=0x4000000
            dst=(a+8+rel)&MASK
            calls.setdefault(dst,[]).append(a)
        # Track direct and PC-relative references including conditional
        # candidates (condition is recorded by disassembly, not asserted).
        if w&0x0f7f0000!=0x051f0000:continue
        seq=list(md.disasm(code[off:off+192],a))
        if not seq:continue
        i=seq[0];o=i.operands
        if len(o)!=2 or o[0].type!=ARM_OP_REG or o[1].type!=ARM_OP_MEM or o[1].mem.base!=ARM_REG_PC:continue
        r=o[0].reg;val=word(a+8+o[1].mem.disp)
        if val is None or r==ARM_REG_PC:continue
        ref(a,val,"literal")
        for j in seq[1:]:
            if j.group(CS_GRP_CALL) or j.group(CS_GRP_JUMP) or j.group(CS_GRP_RET):break
            o=j.operands
            if j.mnemonic in ("add","sub") and len(o)==3 and all(x.type==ARM_OP_REG and not x.shift.type for x in o):
                x,y=o[1].reg,o[2].reg
                if x==ARM_REG_PC and y==r:ref(j.address,(j.address+8+(val if j.mnemonic=="add" else -val))&MASK,"pc-relative")
                if j.mnemonic=="add" and x==r and y==ARM_REG_PC:ref(j.address,(j.address+8+val)&MASK,"pc-relative")
            try:_,writes=j.regs_access()
            except Exception:break
            if r in writes or ARM_REG_PC in writes:break
    # Bound the contexts to the actual log xrefs; report only direct
    # call-target addresses and unwind-based candidates, never personal logs.
    launch=scan(data)
    rows=[]
    for (a,v),row in sorted(refs.items()):
        f=owner(a)
        near=[{"site":hex(q),"target":hex(dst)} for dst,sites in calls.items() for q in sites if abs(q-a)<=256]
        row.update({"unwind_entry_candidate":hex(f) if f is not None else None,
                    "entry_callers":[hex(q) for q in calls.get(f,[])][:40],
                    "nearby_calls":sorted(near,key=lambda x:int(x["site"],16)),
                    "nearby_process_calls":[x for x in launch["calls"] if abs(int(x["site"],16)-a)<=512]})
        rows.append(row)
    return {"program":name,"sha256":sha(data),"strings":{hex(a):k for a,k in strings.items()},
            "unwind_entries":len(entries),"references":rows,"runtime_verified":False}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    report=[trace(get_original(root,n),n) for n in ("smartphone_integrator","dio_manager")]
    out.write_text(json.dumps(report,indent=2)+"\n")
    for r in report:
        print("ORIGINAL_LOG_EVENT_MAP",r["program"],"strings",json.dumps(r["strings"]),"refs",len(r["references"]))
        for x in r["references"][:30]:print("LOG_XREF",json.dumps(x)[:4000])
