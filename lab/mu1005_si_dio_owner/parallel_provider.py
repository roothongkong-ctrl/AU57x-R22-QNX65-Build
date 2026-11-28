#!/usr/bin/env python3
"""Independent exact-firmware provider candidates. No firmware or secrets exported.
No candidate implies a live call, authorization, or installation readiness."""
import bisect,io,json,re,struct,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from launch_provider import scan
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_usb_exit"))
from native_map import get_original,sha
ANCHORS=(0x18bd84,0x18c1d8,0x18c258,0x18c650)
def prel31(w,a):
    w &= 0x7fffffff
    return (a+(w-0x80000000 if w&0x40000000 else w))&0xffffffff
def owner(data):
    e=ELFFile(io.BytesIO(data)); sec=e.get_section_by_name(".text")
    code=sec.data();va=int(sec["sh_addr"]);end=va+len(code)
    ex=e.get_section_by_name(".ARM.exidx")
    if ex is None:
        ex=next((s for s in e.iter_sections() if s["sh_type"]=="SHT_ARM_EXIDX"),None)
    if ex is None:
        return {"unwind_present":False,"anchors":[]}
    b=ex.data();x=int(ex["sh_addr"])
    starts=sorted({prel31(struct.unpack_from("<I",b,o)[0],x+o) for o in range(0,len(b)-7,8)})
    starts=[a for a in starts if va<=a<=end]
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
    out=[]
    for a in ANCHORS:
        j=bisect.bisect_right(starts,a)-1
        if j<0 or j+1>=len(starts):continue
        lo,hi=starts[j:j+2]
        direct=[]
        for o in range(0,len(code)-3,4):
            w=struct.unpack_from("<I",code,o)[0]
            if w&0x0e000000!=0x0a000000 or w>>28==15:continue
            rel=(w&0xffffff)<<2
            if rel&0x2000000:rel-=0x4000000
            p=va+o;t=(p+8+rel)&0xffffffff
            if t==lo and not lo<=p<hi:
                direct.append({"site":hex(p),"kind":"BL" if w&0x01000000 else "B","guard":hex(w)})
        out.append({"anchor":hex(a),"unwind_region":[hex(lo),hex(hi)],"region_entry_candidates":direct[:100],"candidate_count":len(direct),"entry_not_proven":True,"entry_context":[{"va":hex(i.address),"op":i.mnemonic,"args":i.op_str} for i in md.disasm(code[lo-va:min(hi-va,lo-va+64)],lo)]})
    return {"unwind_present":True,"anchors":out}
def arguments(data):
    e=ELFFile(io.BytesIO(data));sec=e.get_section_by_name(".text");va=int(sec["sh_addr"]);code=sec.data()
    p=scan(data); names={int(k,16) for k in p["dio_strings"]}
    loads=[x for x in e.iter_segments() if x["p_type"]=="PT_LOAD"]
    # Look for literal pointer tables (including data/GOT); distinguish
    # pointer location from any actual invocation. No speculative dereference.
    pointers=[]
    for seg in loads:
        b=seg.data();a=int(seg["p_vaddr"])
        for o in range((-a)%4,len(b)-3,4):
            v=struct.unpack_from("<I",b,o)[0]
            if v in names:pointers.append({"slot":hex(a+o),"target":hex(v),"flags":int(seg["p_flags"])})
    targets={int(x["slot"],16) for x in pointers}
    refs=[]
    for o in range(0,len(code)-3,4):
        w=struct.unpack_from("<I",code,o)[0];a=va+o
        if w&0x0f7f0000!=0x051f0000:continue
        slot=a+8+((w&4095) if w&0x00800000 else -(w&4095))
        for seg in loads:
            v=int(seg["p_vaddr"]);b=seg.data()
            if v<=slot and slot+4<=v+len(b):
                q=struct.unpack_from("<I",b,slot-v)[0]
                if q in targets:refs.append({"site":hex(a),"slot":hex(slot),"pointer_table_entry":hex(q),"guard":hex(w)})
                break
    return {"dio_pointer_tables":pointers,"indirect_literal_candidates":refs}
def providers(root,lane):
    pats={"process-provider":r"ProcessTNG|ProcessManager",
          "bluetooth-provider":r"BlueSDK|RFCOMM|RfComm|Bluetooth.*(Register|Connect|Service)|BTM_",
          "wifi-provider":r"hostapd|wpa_supplicant|WiFi.*(AccessPoint|Hotspot|Channel)|WLAN.*(AccessPoint|Hotspot|Channel)|AirPlay.*(Network|Interface)"}
    pat=re.compile(pats[lane],re.I); out=[]
    for f in root.rglob("*"):
        if not f.is_file() or not f.resolve().is_relative_to(root):continue
        try:
            with f.open("rb") as h:
                if h.read(4)!=b"\x7fELF":continue
                h.seek(0);e=ELFFile(h)
                syms=[]
                for s in e.iter_sections():
                    if not isinstance(s,SymbolTableSection) or s["sh_type"]!="SHT_DYNSYM":continue
                    for z in s.iter_symbols():
                        if pat.search(z.name):
                            syms.append({"symbol":z.name,"defined":z["st_shndx"]!="SHN_UNDEF","value":hex(int(z["st_value"]))})
                if syms:out.append({"file":str(f.relative_to(root)),"symbols":syms[:100],"total":len(syms)})
        except (OSError,ValueError):continue
    return {"matches":out[:150],"file_count":len(out),"static_symbols_only":True}
def config(root):
    # Text references only: never output configuration values or file contents.
    patterns={"dio":rb"dio_manager","si":rb"smartphone_integrator","wireless":rb"wireless.?carplay|carplay.?wireless","iap2":rb"iap2"}
    out=[]
    for f in root.rglob("*"):
        if not f.is_file() or not f.resolve().is_relative_to(root):continue
        if f.suffix.lower() not in (".cfg",".conf",".ini",".xml",".json",".sh",".txt",".lua",".properties"):continue
        try:
            if f.stat().st_size>2*1024*1024:continue
            b=f.read_bytes()
            if b"\x00" in b[:4096]:continue
            hits={}
            for k,p in patterns.items():
                lines=[i for i,l in enumerate(b.splitlines(),1) if re.search(p,l,re.I)]
                if lines:hits[k]=lines[:100]
            if hits:out.append({"file":str(f.relative_to(root)),"matching_lines":hits})
        except OSError:continue
    return {"files":out,"file_count":len(out)}
if __name__=="__main__":
    lane,source,dest=sys.argv[1:]
    root=Path(source).resolve(strict=True)
    data=get_original(root,"smartphone_integrator")
    if lane=="unwind-owner":result=owner(data)
    elif lane=="launch-arguments":result=arguments(data)
    elif lane=="config-entry":result=config(root)
    elif lane in ("process-provider","bluetooth-provider","wifi-provider"):result=providers(root,lane)
    else:raise ValueError(lane)
    report={"lane":lane,"si_sha256":sha(data),"result":result,"runtime_verified":False,"installable":False}
    Path(dest).write_text(json.dumps(report,indent=2)+"\n")
    print("R105287_PROVIDER",lane,json.dumps(result)[:24000])
