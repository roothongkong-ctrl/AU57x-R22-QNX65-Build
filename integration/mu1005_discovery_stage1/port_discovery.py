#!/usr/bin/env python3
"""Reuse the pinned donor's real registration and EIR code; discovery only.
This first milestone deliberately rejects incoming RFCOMM connections.
Never activate on a vehicle from this source-only research build.
"""
from pathlib import Path
import subprocess, sys, re
up,out=map(Path,sys.argv[1:3])
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
s=(up/"wireless/graft/graft.c").read_text()
prefix,rest=s.split("static void guard_lock(struct GraftState *s)",1)
mapping={"RF_REGISTER":(0x277b98,0x277a18),"RF_DEREGISTER":(0x277b34,0x2779b4),
"SDP_ADD":(0x2073c4,0x207244),"SDP_REMOVE":(0x207344,0x2071c4),
"EIR_REQUEST":(0x1db648,0x1db4c8),"RF_ACCEPT":(0x2774bc,0x27733c)}
for name,(old,new) in mapping.items():
    prefix,n=re.subn(r"(?im)^(#define "+name+r" .*?)0x"+format(old,"x")+r"u",lambda m:m.group(1)+hex(new)+"u",prefix)
    assert n==1,name
assert prefix.count("#define STATE_ADDR 0x510000u")==1
prefix=prefix.replace("#define STATE_ADDR 0x510000u","#define STATE_ADDR 0x520000u")
assert prefix.count("u8 peer_address[6], peer_valid;")==1
prefix=prefix.replace("u8 peer_address[6], peer_valid;","u8 peer_address[6], peer_valid;\n    u32 resource_mask, cleanup_status, services;")
# Reuse donor's record construction verbatim; no alternate iAP2 implementation.
body=s.split("void graft_init(void) {",1)[1].split("    if (PTHREAD_CREATE",1)[0]
assert body.count("RF_REGISTER(&s->channel, &s->granted_channel, 0)")==1
body=body.replace("RF_REGISTER(&s->channel, &s->granted_channel, 0)","RF_REGISTER(&s->channel, &s->granted_channel, 4)")
def once(a,b):
    global body
    assert body.count(a)==1,a
    body=body.replace(a,b)
once("    s->status = 0xFFFF0002u;","    s->status = 0xFFFF0002u;\n    s->services=services;")
once("    s->protocol[13] = s->granted_channel;","    s->resource_mask=1;\n    if (s->granted_channel<1 || s->granted_channel>6) { s->status=0xffff0003u; withdraw(s); return; }\n    s->protocol[13] = s->granted_channel;")
once("        (void)RF_DEREGISTER(&s->channel, &s->granted_channel);\n        s->status = (u32)status;\n        return;\n    }\n    s->carplay_attribute.id", "        s->status=(u32)status; withdraw(s); return;\n    }\n    s->resource_mask|=2;\n    s->carplay_attribute.id")
once("        (void)SDP_REMOVE(&s->record);\n        (void)RF_DEREGISTER(&s->channel, &s->granted_channel);\n        s->status = (u32)status;\n        return;\n    }","        s->status=(u32)status; withdraw(s); return;\n    }")
code=prefix+r'''
/* SDK initialized evidence is not a lifecycle/lock proof. These functions
 * are called only by the exact offline startup/cleanup trampolines. */
static int sdk_live(void) {
    volatile u32 *os=(volatile u32*)0x2d5644u;
    volatile u8 *bytes=(volatile u8*)os;
    return os[0]==0x2c10d8u && bytes[0x16]==1 && os[1] && os[2] && os[3] && os[4];
}
static void withdraw(struct GraftState *s) {
    __atomic_store_n(&s->discovery_ready,0u,__ATOMIC_RELEASE);
    if (!sdk_live()) { if(s->resource_mask)s->cleanup_status=0xffffffffu; return; }
    if ((s->resource_mask&4) && SDP_REMOVE(&s->carplay_record)==0) s->resource_mask&=~4u;
    if ((s->resource_mask&2) && SDP_REMOVE(&s->record)==0) s->resource_mask&=~2u;
    if ((s->resource_mask&1) && RF_DEREGISTER(&s->channel,&s->granted_channel)==0) s->resource_mask&=~1u;
    s->cleanup_status=s->resource_mask;
}
void mu_phase1_cleanup(void *os) {
    if ((u32)os==0x2d5644u) withdraw(ST);
}
void graft_rf_callback(void *channel,const struct RfEvent *event) {
    struct GraftState *s=ST;
    /* Discovery milestone: deliberately reject every incoming RFCOMM
     * request, never send, authenticate, or change the wired path. */
    if (channel==&s->channel && event && event->code==1u && sdk_live())
        (void)RF_ACCEPT(channel,0);
}
void mu_phase1_start(u32 services) {
    if (!services || (services&3u) || !sdk_live() ||
        *(volatile u32*)services!=0x2be500u ||
        *(volatile u8*)(services+0x1b0u)!=1u) return;
'''+body+r'''
    s->resource_mask|=4;
    s->status=0;
    __atomic_store_n(&s->discovery_ready,1u,__ATOMIC_RELEASE);
    (void)EIR_REQUEST();
}
/* Original EIR callsite r6=BluetoothServices, r7=buffer, r2=length.
 * Don't append to a different services instance or malformed list. */
u32 mu_phase1_eir(u8 *eir,u32 length,u32 services) {
    struct GraftState *s=ST;
    if (services!=s->services || (u32)eir!=services+0x31cu || !sdk_live() ||
        length==0 || length>=240 || !__atomic_load_n(&s->discovery_ready,__ATOMIC_ACQUIRE))return length;
    /* Preserve the original EIR if any field is malformed or incomplete. */
    for(u32 i=0;i<length;) {
        u32 n=eir[i];
        if(!n || n>length-i-1u || eir[i+1]==6 || (eir[i+1]==7 && (n-1u)%16u)) return length;
        i+=n+1;
    }
    return graft_augment_eir(eir,length);
}
'''
out.mkdir(parents=True,exist_ok=False)
(out/"discovery.c").write_text(code)
(out/"eir.c").write_bytes((up/"wireless/graft/eir.c").read_bytes())
print("MU1005_DIRECT_DONOR_REGISTRATION_PORT_NO_INSTALL")
