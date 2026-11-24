#!/usr/bin/env python3
"""Source-level port of pinned upstream graft. Compile review only; no OEM input or patch."""
import pathlib,re,sys
up=pathlib.Path(sys.argv[1])/"wireless/graft"
out=pathlib.Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
s=(up/"graft.c").read_text()
mapping={
"RF_REGISTER":(0x277b98,0x277a18),"RF_DEREGISTER":(0x277b34,0x2779b4),
"SDP_ADD":(0x2073c4,0x207244),"SDP_REMOVE":(0x207344,0x2071c4),
"EIR_REQUEST":(0x1db648,0x1db4c8),"RF_ACCEPT":(0x2774bc,0x27733c),
"RF_DISCONNECT":(0x277578,0x2773f8),"RF_SEND":(0x277378,0x2771f8),
"RF_MAX_PAYLOAD":(0x2771f4,0x277074),"RF_CREDIT":(0x2772dc,0x27715c),
"PTHREAD_CREATE":(0x1173fc,0x117390),"SOCKET":(0x117618,0x1175ac),
"CONNECT":(0x117660,0x1175f4),"POLL":(0x116d00,0x116ca0),
"SEND":(0x117318,0x1172b8),"RECV":(0x117be8,0x117b7c),
"CLOSE":(0x1174b0,0x117444),"USLEEP":(0x116e74,0x116e14)}
def once(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b)
for name,(old,new) in mapping.items():
 pat=r"(?im)^(#define "+name+r" .*?)0x"+format(old,"x")+r"u"
 s,n=re.subn(pat,lambda m:m.group(1)+hex(new)+"u",s)
 assert n==1,name
once("#define STATE_ADDR 0x510000u","#define STATE_ADDR 0x520000u")
once('ldr pc, =0x12a714','ldr pc, =0x12a6a4')
once('ldr pc, =0x11ecd4','ldr pc, =0x11ec64')
# MU1005 register arg3 initializes credit fields +0x8b/+0x86, not SCN.
# Evidence: 0x277a6c/74, 0x209b40..64, 0x20b3a8..d0.
# Slot byte arg2 is independent and automatically allocated. No live attach.
# A separate deny-only translation unit keeps full compiled code present without
# granting permission; no authentication is replaced or simulated.
once("void graft_init(void) {","extern int au57x_review_owner(unsigned short *);\nvoid graft_init(void) {\n    u16 initial_credits=0;\n    if (au57x_review_owner(&initial_credits)!=1 || initial_credits<1 || initial_credits>7) return;")
once("RF_REGISTER(&s->channel, &s->granted_channel, 0)","RF_REGISTER(&s->channel, &s->granted_channel, initial_credits)")
# The initial credit grant is already part of the native negotiation; avoid
# granting four more on each CONNECTED record. Refuse zero-peer snapshots.
once("                    (void)RF_CREDIT(&s->channel, 4);",
     "                    /* Initial credits negotiated by RF_REGISTER. */")
once("                    (void)RF_CREDIT(&s->channel, 1);",
     "                    if (RF_CREDIT(&s->channel, 1) != 0) goto bridge_broken;")
once("        if (!queue_push(s, FRAME_CONNECTED, s->generation,",
     "        if (!s->peer_valid || !queue_push(s, FRAME_CONNECTED, s->generation,")
once("            snapshot_pending = s->connected != 0;",
     "            if (s->connected) { guard_unlock(s); goto bridge_broken; }\n            snapshot_pending = 0;")
once("        __atomic_store_n(&s->tx_busy, 0u, __ATOMIC_RELEASE);\n    }\n}",
     "        if (event->status != 0) __atomic_store_n(&s->overflow, 1u, __ATOMIC_RELEASE);\n        __atomic_store_n(&s->tx_busy, 0u, __ATOMIC_RELEASE);\n    }\n}")
s='#ifndef AU57X_LINK_REVIEW_ONLY\n#error Review only; do not attach to any OEM process\n#endif\n'+s
(out/"graft.c").write_text(s)
(out/"eir.c").write_text((up/"eir.c").read_text())
ld=(up/"graft.ld").read_text()
assert ld.count("0x508000")==1
(out/"graft.ld").write_text(ld.replace("0x508000","0x510000"))
(out/"deny.c").write_text("int au57x_review_owner(unsigned short *credits) { if (credits) *credits=0; return 0; }\n")
print("MU1005_PORT_SOURCE_18_ADDRESSES_2_RESUMES_REVIEW_ONLY")
