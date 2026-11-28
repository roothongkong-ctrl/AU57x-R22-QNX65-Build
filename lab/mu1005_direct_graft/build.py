#!/usr/bin/env python3
"""Direct LuKa graft port to an exact private MU1005 copy.
Registration build only: remote accepts remain disabled until the existing
MU1005 native security callback and supervised transport are connected.
No original bytes or modified OEM executable are exported from CI.
"""
import hashlib,json,re,shutil,struct,subprocess,sys,tempfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from io import BytesIO
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
from mu1005_lifecycle_observer import PIN,CODE,ENTRY,STATE,headers,branch,align
DONOR="20c94f51c0f0ae88b154c12e4f604643ba23022c"
HOOK=0x11ec98
STOCK_WORD=0xe5c531b0
MAP={"RF_REGISTER":0x277a18,"RF_DEREGISTER":0x2779b4,"SDP_ADD":0x207244,
     "SDP_REMOVE":0x2071c4,"EIR_REQUEST":0x1db4c8,"RF_ACCEPT":0x27733c,
     "RF_DISCONNECT":0x2773f8,"RF_SEND":0x2771f8,"RF_MAX_PAYLOAD":0x277074,
     "RF_CREDIT":0x27715c}
IMPORTS={"PTHREAD_CREATE":"pthread_create","SOCKET":"socket","CONNECT":"connect",
         "POLL":"poll","SEND":"send","RECV":"recv","CLOSE":"close","USLEEP":"usleep"}
def sha(b):return hashlib.sha256(b).hexdigest()
def need(ok,msg):
    if not ok:raise ValueError(msg)
def rep(s,a,b):
    need(s.count(a)==1,"upstream anchor "+a[:100])
    return s.replace(a,b)
def word(b,a):
    for k,o,v,_,f,m,flags,al in headers(b):
        if k==1 and v<=a and a+4<=v+f:return struct.unpack_from("<I",b,o+a-v)[0]
    raise ValueError("unmapped "+hex(a))
def imm(w):
    n=w&255;s=((w>>8)&15)*2
    return ((n>>s)|(n<<(32-s)))&0xffffffff if s else n
def imports(b):
    elf=ELFFile(BytesIO(b))
    slots={}
    for sec in elf.iter_sections():
        if isinstance(sec,RelocationSection):
            sym=elf.get_section(sec["sh_link"])
            for r in sec.iter_relocations():
                if r["r_info_type"]==22 and r["r_info_sym"]:
                    slots[int(r["r_offset"])]=sym.get_symbol(r["r_info_sym"]).name
    p=elf.get_section_by_name(".plt")
    need(p is not None,"missing PLT")
    d=p.data();v=int(p["sh_addr"]);found={}
    for off in range(0,len(d)-11,4):
        a,c,e=struct.unpack_from("<III",d,off)
        if a&0xfffff000!=0xe28fc000 or c&0xfffff000!=0xe28cc000 or e&0xfffff000!=0xe5bcf000:continue
        slot=(v+off+8+imm(a)+imm(c)+(e&4095))&0xffffffff
        if slot in slots:found.setdefault(slots[slot],[]).append(v+off)
    result={}
    for macro,name in IMPORTS.items():
        matches=found.get(name,[])
        need(len(matches)==1,"missing/ambiguous PLT "+name)
        result[macro]=matches[0]
    return result
def adapt(s,m):
    s=rep(s,"#define STATE_ADDR 0x510000u",f"#define STATE_ADDR {STATE:#x}u")
    for macro,va in m.items():
        pattern=rf"(#define {macro} \([^\n]+?)0x[0-9A-Fa-f]+u(\)[^\n]*\n)"
        s,n=re.subn(pattern,lambda x:x[1]+f"{va:#x}u"+x[2],s)
        need(n==1,"upstream macro "+macro)
    s=rep(s,"RF_REGISTER(&s->channel, &s->granted_channel, 0)","RF_REGISTER(&s->channel, &s->granted_channel, 4)")
    # Keep the entire donor registration, callback and worker in the candidate,
    # but do not accept a handset on the unverified direct callback.
    s=rep(s,"(void)RF_ACCEPT(channel, ready);","(void)ready; (void)RF_ACCEPT(channel, 0); /* review only; OEM security bridge not yet wired */")
    # MU1005 registration gives four initial credits; donor originally
    # passed zero and refilled four on first CONNECTED. Do not double grant.
    s=rep(s,"(void)RF_CREDIT(&s->channel, 4);","/* MU1005 initial credits already granted by registration */")
    # This isolated build has no genuine accepted session. No later event
    # may produce a false connected notification or forward handset data.
    s=rep(s,"    if (event->code == 2u) {","    if (event->code == 2u) { return; /* no authorized connection in review build */")
    # The old EIR trampoline is firmware-specific; do not install it.
    # The registered SDP records and original refresh are reused.
    start=s.index("/* At 0x12A710:")
    s=s[:start]+r'''
__attribute__((naked,used)) void graft_hook(void) {
    __asm__ volatile(
        "strb r3, [r5,#0x1b0]\n"
        "stmdb sp!, {r0-r3,r12,lr}\n"
        "mrs r12,cpsr\n"
        "stmdb sp!, {r12,lr}\n"
        "bl graft_init\n"
        "ldmia sp!, {r12,lr}\n"
        "msr cpsr_f,r12\n"
        "ldmia sp!, {r0-r3,r12,lr}\n"
        "ldr pc, =0x11ec9c\n"
    );
}
'''
    return s
def compile_graft(source,donor):
    with tempfile.TemporaryDirectory() as tmp:
        t=Path(tmp)
        (t/"graft.c").write_text(source)
        # eir.c is reused for the donor's helper; the original hook is omitted.
        for name,src in [("graft",t/"graft.c"),("eir",donor/"wireless/graft/eir.c")]:
            subprocess.run(["clang","-target","armv7-none-eabi","-march=armv7-a","-marm","-O2","-ffreestanding","-fno-builtin","-fno-unwind-tables","-fno-asynchronous-unwind-tables","-fno-stack-protector","-fno-pic","-ffunction-sections","-fdata-sections","-c",str(src),"-o",str(t/(name+".o"))],check=True)
        (t/"graft.ld").write_text(f"ENTRY(graft_hook)\nSECTIONS {{ . = {ENTRY:#x}; .graft : {{ *(.text*) *(.rodata*) *(.data*) }} /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) }} }}\n")
        linker=shutil.which("ld.lld") or shutil.which("ld.lld-18")
        need(linker is not None,"ARM linker missing; install lld")
        subprocess.run([linker,"-flavor","gnu","-m","armelf","-T",str(t/"graft.ld"),"--no-undefined",str(t/"graft.o"),str(t/"eir.o"),"-o",str(t/"graft.elf")],check=True)
        elf=ELFFile(BytesIO((t/"graft.elf").read_bytes()))
        sec=elf.get_section_by_name(".graft")
        code=sec.data()
        syms=elf.get_section_by_name(".symtab")
        hook=int(syms.get_symbol_by_name("graft_hook")[0]["st_value"])
        need(sec["sh_addr"]==ENTRY and ENTRY+len(code)<STATE,"graft placement")
        need(ENTRY<=hook<ENTRY+len(code),"hook placement")
        need(not any(isinstance(x,RelocationSection) for x in elf.iter_sections()),"unresolved relocations")
    return code,hook

def source_build(donor,out):
    need(subprocess.check_output(["git","-C",str(donor),"rev-parse","HEAD"],text=True).strip()==DONOR,"wrong donor")
    # Compile/link same source transformations without re-extracting the
    # original firmware. Placeholder PLTs are never executed or exported.
    m=MAP|{name:0x100000+i*4 for i,name in enumerate(IMPORTS)}
    source=adapt((donor/"wireless/graft/graft.c").read_text(),m)
    code,hook=compile_graft(source,donor)
    out.write_text(json.dumps(dict(scope="SOURCE_LINK_ONLY_NO_ORIGINAL_PLT_NO_RUNTIME",code_size=len(code),hook=hex(hook),installable=False),indent=2)+"\n")
    print("DIRECT_DONOR_SOURCE_LINK_PASS_NO_RUNTIME")

def main(stock,donor,out,private):
    b=stock.read_bytes()
    need(sha(b)==PIN,"wrong original")
    need(subprocess.check_output(["git","-C",str(donor),"rev-parse","HEAD"],text=True).strip()==DONOR,"wrong donor")
    old=headers(b)
    need([x[0] for x in old]==[6,3,1,1,2],"ELF layout")
    need(all(x[2]+x[5]<=CODE for x in old if x[0]==1),"RX overlap")
    need(word(b,HOOK)==STOCK_WORD,"startup instruction")
    guards=(ROOT/"project/source/legacy_mu1005/bluetooth/native_binding_words.h").read_text()
    checked={int(a,16):int(w,16) for a,w in re.findall(r"\{(0x[0-9a-f]+)u,(0x[0-9a-f]+)u\}",guards)}
    for a,w in checked.items():need(word(b,a)==w,"native guard "+hex(a))
    need(word(b,MAP["EIR_REQUEST"])==0xe92d4010,"EIR guard")
    m=MAP|imports(b)
    source=adapt((donor/"wireless/graft/graft.c").read_text(),m)
    code,hook=compile_graft(source,donor)
    # Same original PHDR layout strategy as the existing MU1005 observer.
    n=7;off=align(len(b),4096);payload=bytearray(0x100);payload.extend(code)
    stateoff=align(off+len(payload),4096)
    new=[x[:] for x in old];new[0]=[6,off,CODE,CODE,n*32,n*32,4,4]
    new.extend([[1,off,CODE,CODE,len(payload),len(payload),5,4096],[1,stateoff,STATE,STATE,0,0x20000,6,4096]])
    payload[:n*32]=b"".join(struct.pack("<8I",*x) for x in new)
    result=bytearray(b);struct.pack_into("<I",result,28,off);struct.pack_into("<H",result,44,n)
    struct.pack_into("<I",result,HOOK-0x100000,branch(HOOK,hook))
    allowed=set(range(28,32))|set(range(44,46))|set(range(HOOK-0x100000,HOOK-0x100000+4))
    need(all(i in allowed for i,(a,c) in enumerate(zip(b,result)) if a!=c),"unexpected OEM mutation")
    result.extend(bytes(off-len(result)));result.extend(payload);result.extend(bytes(stateoff-len(result)))
    need(headers(result)==new and stock.read_bytes()==b,"postbuild validation")
    private.write_bytes(result)
    out.write_text(json.dumps(dict(scope="DIRECT_DONOR_REGISTRATION_BUILD_REVIEW_ONLY",original_sha256=PIN,generated_sha256=sha(result),donor=DONOR,startup=hex(HOOK),hook=hex(hook),resume="0x11ec9c",native={k:hex(v) for k,v in m.items()},native_guard_count=len(checked),code_size=len(code),original_loads_preserved=True,remote_accept=False,installable=False,vehicle_verified=False,runtime_verified=False,remaining=["native security callback and paired authorization","supervised endpoint and wire ACK","lifecycle stop and wired handoff"]),indent=2)+"\n")
    print("DIRECT_MU1005_DONOR_STARTUP_BUILD_ONLY_NO_INSTALL")
if __name__=="__main__":
    if len(sys.argv)==4 and sys.argv[1]=="--source-build":
        source_build(Path(sys.argv[2]).resolve(),Path(sys.argv[3]).resolve())
    else:
        main(*(Path(x).resolve() for x in sys.argv[1:]))
