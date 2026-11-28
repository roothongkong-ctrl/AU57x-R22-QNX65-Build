#!/usr/bin/env python3
"""Direct LuKa graft port to an exact private MU1005 copy.
Registration review: original MU1005 security checks are integrated into the
callback. Remote acceptance remains disabled until supervised transport exists.
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
# Single retained MU1005 mapping for non-exported original functions.
# Standard imported calls are resolved from the original ELF by imports().
_PORT=json.loads((ROOT/"integration/mu1005_vehicle_stage/congo_port_table.json").read_text())
if _PORT["stock_btstack_sha256"]!=PIN:raise ValueError("port table firmware mismatch")
_ROW={e["name"]:e for e in _PORT["entries"]}
_NAMES={"RF_REGISTER":"RF_RegisterServerChannel","RF_DEREGISTER":"RF_DeregisterServerChannel",
        "SDP_ADD":"SDP_AddRecord","SDP_REMOVE":"SDP_RemoveRecord",
        "EIR_REQUEST":"EIR_refresh","RF_ACCEPT":"RF_RespondChannel",
        "RF_DISCONNECT":"RF_CloseChannel","RF_SEND":"RF_SendData",
        "RF_MAX_PAYLOAD":"RF_FrameSize","RF_CREDIT":"RF_AdvanceCredit"}
MAP={k:int(_ROW[v]["stock_va"],16) for k,v in _NAMES.items()}
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
def original_security_source():
    # Reuse the existing audited pure functions, not an invented security policy.
    p=ROOT/"project/source/legacy_mu1005/bluetooth/native_event.c"
    text=p.read_text()
    lo=text.index("/* Original ET_EXEC bt symbol")
    # Only these three pure helpers and the original registry/bond/security checks.
    prefix=text[text.index("static unsigned le16("):text.index("int mu38_resolve_peer(")]
    local=r'''
typedef u8 uint8_t;
typedef u32 uint32_t;
typedef unsigned size_t;
#define UINT32_MAX 0xffffffffu
static int mu_memcmp(const void *a,const void *b,size_t n){
    const u8*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return (int)x[i]-(int)y[i];return 0;
}
#define memcmp mu_memcmp
'''
    return local+prefix+text[lo:]+r'''
/* Direct graft: only called at the original RFCOMM SDK callback boundary.
 * The fixed stock reads below are restricted to the exact previously pinned
 * code/data, or our own channel object. Never read stored Bluetooth keys.
 * Review only: callback lifetime, supervisor transport and shutdown must
 * be integrated before any remote acceptance is enabled. */
static int mu_graft_read(void *ctx,u32 address,void *out,unsigned n) {
    struct GraftState *s=ctx;
    u32 ch=(u32)&s->channel;
    if(!n || address>0xffffffffu-(n-1))return 0;
    int permitted=(address>=0x100000u && address<0x2b97dcu && n<=0x2b97dcu-address) ||
       (address>=0x2c0000u && address<0x508780u && n<=0x508780u-address) ||
       (address>=ch && address-ch<sizeof s->channel && n<=sizeof s->channel-(address-ch));
    if(!permitted)return 0;
    bytes_copy(out,(const u8*)address,n);
    return 1;
}
static int mu_graft_peer(struct GraftState*s,const struct RfEvent*e,u8 mac[6]) {
    u32 a=(u32)e->data;
    if(!a || (a&3u) || a>0xffffffffu-18u ||
       !mu38_registry_association((u32)&s->channel,a,mu_graft_read,s))return 0;
    /* Do not dereference an arbitrary heap address. The existing approved
     * reader and native lifetime provider must be connected before activation. */
    if(!mu_graft_read(s,a+13u,mac,6))return 0;
    return mu38_stock_bond_present(mac,mu_graft_read,s) &&
        mu38_registry_association((u32)&s->channel,a,mu_graft_read,s);
}

'''

def adapt(s,m):
    s=rep(s,"#define STATE_ADDR 0x510000u",f"#define STATE_ADDR {STATE:#x}u")
    for macro,va in m.items():
        pattern=rf"(#define {macro} \([^\n]+?)0x[0-9A-Fa-f]+u(\)[^\n]*\n)"
        s,n=re.subn(pattern,lambda x:x[1]+f"{va:#x}u"+x[2],s)
        need(n==1,"upstream macro "+macro)
    s=rep(s,"RF_REGISTER(&s->channel, &s->granted_channel, 0)","RF_REGISTER(&s->channel, &s->granted_channel, 4)")
    # Preserve donor state layout by appending the new fields.
    s=rep(s,"    u8 peer_address[6], peer_valid;","    u8 peer_address[6], peer_valid;\n    volatile u32 transport_owner_verified;\n    u32 pending_association;")
    # Reuse the real MU1005 original security callback and paired-record check.
    # No writer exists for transport_owner_verified yet: actual remote acceptance
    # remains disabled until the independently supervised endpoint is wired.
    s=rep(s,"void graft_rf_callback(void *channel, const struct RfEvent *event) {",original_security_source()+r'''
static void graft_rf_dispatch(void*,const struct RfEvent*,u32,u32) __attribute__((used,noinline));
__attribute__((naked,noinline)) void graft_rf_callback(void *channel __attribute__((unused)),const struct RfEvent *event __attribute__((unused))) {
    __asm__ volatile(
        "mov r2, lr\n"
        "mov r3, #0\n"
        "ldr r12, =0x203d54\n"
        "cmp lr, r12\n"
        "ldreq r3, [sp, #44]\n"
        "b graft_rf_dispatch\n"
    );
}
static void graft_rf_dispatch(void *channel,const struct RfEvent *event,u32 lr,u32 caller) {
''')
    s=rep(s,"        (void)RF_ACCEPT(channel, ready);",r'''        u8 mac[6];
        int allow=ready && !event->status && !s->connected && !s->pending_association &&
            __atomic_load_n(&s->transport_owner_verified,__ATOMIC_ACQUIRE)==1u &&
            mu38_stock_rf_security_request((u32)channel,lr,caller,mu_graft_read,s) &&
            mu_graft_peer(s,event,mac);
        if(allow)s->pending_association=(u32)event->data;
        if(RF_ACCEPT(channel,allow)!=2){s->pending_association=0;__atomic_store_n(&s->overflow,1u,__ATOMIC_RELEASE);}
''')
    # MU1005 registration gives four initial credits; donor originally
    # passed zero and refilled four on first CONNECTED. Do not double grant.
    s=rep(s,"(void)RF_CREDIT(&s->channel, 4);","/* MU1005 initial credits already granted by registration */")
    s=rep(s,"    if (event->code == 2u) {",r'''    if (event->code == 2u) {
        u8 mac[6];
        if(event->status || lr!=0x203d54u || !s->pending_association ||
           s->pending_association!=(u32)event->data ||
           __atomic_load_n(&s->transport_owner_verified,__ATOMIC_ACQUIRE)!=1u ||
           !mu_graft_peer(s,event,mac)) {s->pending_association=0;return;}
        s->pending_association=0;
''')
    s=rep(s,"        s->peer_valid = event->data != 0;\n        if (s->peer_valid) bytes_copy(s->peer_address,(const u8 *)event->data+13,6);","        s->peer_valid = 1;\n        bytes_copy(s->peer_address,mac,6);")
    s=rep(s,"    if (event->code == 4u) {","    if (event->code == 4u) { s->pending_association=0;")
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
    return inherited_transport(s)
def inherited_transport(s):
    s=rep(s,"#define FRAME_DISCONNECTED 3u","#define FRAME_DISCONNECTED 3u\n#define FRAME_TX_COMPLETE 4u")
    s=rep(s,"    u32 pending_association;","    u32 pending_association;\n    volatile u32 supervised_fd_token;")
    s=rep(s,"    const struct SockAddrIn endpoint = {\n        16, 2, (u8)(BRIDGE_PORT >> 8), (u8)BRIDGE_PORT,\n        {127,0,0,1}, {0,0,0,0}\n    };\n","")
    s=rep(s,"    u32 tx_length = 0, tx_offset = 0, tx_generation = 0;","    u32 tx_length = 0, tx_offset = 0, tx_generation = 0, ack_generation = 0;")
    s=rep(s,"            fd = SOCKET(2, 1, 0);\n            if (fd < 0 || CONNECT(fd, &endpoint, 16) != 0) {\n                if (fd >= 0) (void)CLOSE(fd);\n                fd = -1;\n                (void)USLEEP(200000);\n                continue;\n            }",r'''            /* Consume an already connected endpoint exclusively, once.
             * The existing supervisor owns startup/READY and descriptor proof.
             * Review build has no publisher; it never opens another listener
             * or connects to an arbitrary process on the loopback port. */
            u32 token=__atomic_exchange_n(&s->supervised_fd_token,0u,__ATOMIC_ACQ_REL);
            if(token<4u || token>0x7fffffffu){
                (void)USLEEP(200000);
                continue;
            }
            fd=(int)(token-1u);''')
    # Acknowledgements follow all actual chunk completions, not successful send().
    s=rep(s,"    if (event->code == 6u && event->data == &s->tx_packet) {\n        __atomic_store_n(&s->tx_busy, 0u, __ATOMIC_RELEASE);\n    }",r'''    if (event->code == 6u && event->data == &s->tx_packet) {
        if(event->status || !__atomic_load_n(&s->tx_busy,__ATOMIC_ACQUIRE))
            __atomic_store_n(&s->overflow,1u,__ATOMIC_RELEASE);
        __atomic_store_n(&s->tx_busy, 0u, __ATOMIC_RELEASE);
    }''')
    s=rep(s,"        /* A daemon frame is a byte stream, not one RFCOMM packet.",r'''        if (ack_generation && !__atomic_load_n(&s->tx_busy,__ATOMIC_ACQUIRE)) {
            guard_lock(s);
            int ack_ok=s->connected && s->generation==ack_generation &&
                queue_push(s,FRAME_TX_COMPLETE,ack_generation,0,0);
            guard_unlock(s);
            if(!ack_ok)goto bridge_broken;
            ack_generation=0;
        }

        /* A daemon frame is a byte stream, not one RFCOMM packet.''')
    s=rep(s,"                if (tx_offset == tx_length) {\n                    tx_length = tx_offset = 0;","                if (tx_offset == tx_length) {\n                    ack_generation=tx_generation;\n                    tx_length = tx_offset = 0;")
    s=rep(s,"        if (!tx_length && !__atomic_load_n(&s->tx_busy, __ATOMIC_ACQUIRE))","        if (!tx_length && !ack_generation && !__atomic_load_n(&s->tx_busy, __ATOMIC_ACQUIRE))")
    s=rep(s,"bridge_broken:\n        __atomic_store_n(&s->bridge_ready, 0u, __ATOMIC_RELEASE);","bridge_broken:\n        __atomic_store_n(&s->transport_owner_verified,0u,__ATOMIC_RELEASE);\n        ack_generation=0;\n        __atomic_store_n(&s->bridge_ready, 0u, __ATOMIC_RELEASE);")
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
    # Reuse the original security anchor table from native_event.c.
    sec=(ROOT/"project/source/legacy_mu1005/bluetooth/native_event.c").read_text().split("int mu38_stock_rf_security_request(",1)[1]
    for a,w in re.findall(r"\{(0x[0-9a-f]+)u,(0x[0-9a-f]+)u\}",sec):
        need(word(b,int(a,16))==int(w,16),"original security guard "+a)
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
    out.write_text(json.dumps(dict(scope="DIRECT_DONOR_REGISTRATION_BUILD_REVIEW_ONLY",original_sha256=PIN,generated_sha256=sha(result),donor=DONOR,startup=hex(HOOK),hook=hex(hook),resume="0x11ec9c",native={k:hex(v) for k,v in m.items()},native_guard_count=len(checked),code_size=len(code),original_loads_preserved=True,remote_accept=False,installable=False,vehicle_verified=False,runtime_verified=False,security_callback_source_integrated=True,inherited_endpoint_consumer=True,completion_ack_source=True,remaining=["actual supervisor FD publisher and owner-thread dispatch","callback lifetime and original shutdown/wired handoff"]),indent=2)+"\n")
    print("DIRECT_MU1005_DONOR_STARTUP_BUILD_ONLY_NO_INSTALL")
if __name__=="__main__":
    if len(sys.argv)==4 and sys.argv[1]=="--source-build":
        source_build(Path(sys.argv[2]).resolve(),Path(sys.argv[3]).resolve())
    else:
        main(*(Path(x).resolve() for x in sys.argv[1:]))
