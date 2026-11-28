#!/usr/bin/env python3
"""Build a private, exact-image SI event observation copy.
No original binary or patched binary may be published. USB events are
generic and old native record ID is not a phone identity or MFi proof."""
import hashlib,io,json,shutil,struct,subprocess,sys,tempfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
from native_map import get_original
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
from mu1005_lifecycle_observer import align,headers,branch,need
POINTS=((0x15d4d0,0xe1a02001,"mu_usb_remove"),
        (0x15d4e4,0xe1a02001,"mu_usb_insert"),
        (0x188030,0xe584a498,"mu_si_reset"))
def digest(b):return hashlib.sha256(b).hexdigest()
def patch(b):
    elf=ELFFile(io.BytesIO(b))
    need(elf.elfclass==32 and elf.little_endian and elf["e_machine"]=="EM_ARM" and elf["e_type"]=="ET_EXEC","original ABI")
    old=headers(b)
    loads=[x for x in old if x[0]==1]
    need(bool(loads),"missing loads")
    def offset(a):
        matches=[p[1]+a-p[2] for p in loads if p[6]&1 and p[2]<=a and a+4<=p[2]+p[4]]
        need(len(matches)==1,"nonunique original code")
        return matches[0]
    for a,w,_ in POINTS:need(struct.unpack_from("<I",b,offset(a))[0]==w,"original guard "+hex(a))
    # Keep all old LOADs unchanged, append separate RX and RW ranges.
    code_va=align(max(p[2]+p[5] for p in loads),65536)
    entry=code_va+512
    state=code_va+65536
    need(state+4096<=0xffffffff,"address range")
    need(sum(p[0]==6 for p in old)<=1,"multiple PT_PHDR")
    with tempfile.TemporaryDirectory() as tmp:
        t=Path(tmp)
        source=(Path(__file__).with_name("si_event_hooks.S")).read_text().replace("EVENT_STATE",hex(state))
        (t/"hooks.s").write_text(source)
        subprocess.run(["clang","-target","armv7-none-eabi","-march=armv7-a","-marm","-c",str(t/"hooks.s"),"-o",str(t/"hooks.o")],check=True)
        linker=shutil.which("ld.lld") or shutil.which("ld.lld-18")
        need(bool(linker),"missing linker")
        subprocess.run([linker,"-flavor","gnu","-m","armelf",f"-Ttext={entry:#x}","--entry=mu_usb_remove","--no-undefined",str(t/"hooks.o"),"-o",str(t/"hooks.elf")],check=True)
        linked=ELFFile(io.BytesIO((t/"hooks.elf").read_bytes()))
        sec=linked.get_section_by_name(".text")
        code=sec.data()
        need(int(sec["sh_addr"])==entry and entry+len(code)<state,"hook placement")
        tab=linked.get_section_by_name(".symtab")
        syms={name:int(tab.get_symbol_by_name(name)[0]["st_value"]) for _,_,name in POINTS}
        need(all(entry<=a<entry+len(code) and a%4==0 for a in syms.values()),"hook symbol range")
        need("There are no relocations" in subprocess.check_output(["readelf","-rW",str(t/"hooks.elf")],text=True),"unresolved relocations")
    n=len(old)+2+(not any(p[0]==6 for p in old))
    table_len=n*32
    need(table_len<=512,"header table overlap")
    off=align(len(b),4096)
    payload=bytearray(512);payload.extend(code)
    stateoff=align(off+len(payload),4096)
    phdr=[6,off,code_va,code_va,table_len,table_len,4,4]
    new=[p[:] for p in old]
    if any(p[0]==6 for p in old):new[next(i for i,p in enumerate(old) if p[0]==6)]=phdr
    else:new.insert(0,phdr)
    new.extend([[1,off,code_va,code_va,len(payload),len(payload),5,4096],
                [1,stateoff,state,state,0,4096,6,4096]])
    payload[:table_len]=b"".join(struct.pack("<8I",*p) for p in new)
    out=bytearray(b);struct.pack_into("<I",out,28,off);struct.pack_into("<H",out,44,n)
    allow=set(range(28,32))|set(range(44,46));patches=[]
    for a,w,name in POINTS:
        o=offset(a);v=branch(a,syms[name]);struct.pack_into("<I",out,o,v)
        allow.update(range(o,o+4))
        patches.append({"site":hex(a),"original":hex(w),"branch":hex(v),"target":hex(syms[name])})
    need(all(i in allow for i,(x,y) in enumerate(zip(b,out)) if x!=y),"unexpected original change")
    out.extend(bytes(off-len(out)));out.extend(payload);out.extend(bytes(stateoff-len(out)))
    need(headers(out)==new,"PHDR verify")
    need([p for p in new if p[0]==1][:len(loads)]==loads,"original LOAD changes")
    # Inspect actual private output with readelf; only report metadata.
    with tempfile.TemporaryDirectory() as tmp:
        p=Path(tmp)/"SI-private-review";p.write_bytes(out)
        subprocess.run(["readelf","-h","-lW",str(p)],check=True,capture_output=True)
    return {"original_sha256":digest(b),"private_review_sha256":digest(out),"patches":patches,
      "event_state_va":hex(state),"event_state_offsets":{"generic_usb_remove_count":0,"generic_usb_insert_count":4,
      "si_reset_count":8,"prior_native_record_id":12,"reset_writer_busy":16,"invalid_observation":20},
      "original_loads_unchanged":True,"original_instruction_replayed":True,
      "loader_verified":False,"runtime_verified":False,"same_phone_verified":False,"installable":False}
if __name__=="__main__":
    root=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2])
    b=get_original(root,"smartphone_integrator")
    r=patch(b);out.write_text(json.dumps(r,indent=2)+"\n")
    print("SI_ORIGINAL_EVENT_PRODUCER_PRIVATE_BUILD",json.dumps(r))
