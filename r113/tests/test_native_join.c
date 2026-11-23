#include "mu1005_native_gate_adapter.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int owner,phone,mfi,usb,visible,quiet;unsigned scn,reg,add,del,unreg;} F;
static F *f;
static int one(void *p){(void)p;return 1;}
static int ow(void *p){return ((F*)p)->owner;}
static int ph(void *p){return ((F*)p)->phone;}
static int mf(void *p){return ((F*)p)->mfi;}
static int wi(void *p){return ((F*)p)->usb;}
static int rg(void *p,uint8_t *c,uint16_t x){(void)p;assert(x==0);f->reg++;*c=(uint8_t)f->scn;return 0;}
static int ur(void *p,uint8_t *c){(void)p;assert(*c==f->scn);f->unreg++;return 0;}
static int ad(void *p){(void)p;f->add++;return 0;}
static int de(void *p){(void)p;f->del++;return 0;}
static int ac(void *p,int x){(void)p;(void)x;return 2;}
static int li(void *p){(void)p;return 50;}
static int se(void *p,void *x){(void)p;(void)x;return 2;}
static int di(void *p){(void)p;return 0;}
static int op(void *p,unsigned x){(void)p;(void)x;return 1;}
static int cl(void *p){(void)p;return 1;}
static int rx(void *p,const uint8_t *x,unsigned n){(void)p;(void)x;(void)n;return 1;}
static int cr(void *p,int x){(void)p;(void)x;return 0;}
static uint32_t addr(const void *p){uint32_t n=(uint32_t)(uintptr_t)p;return n?n:1;}
static int vis(void *p,const Inbound *s,uint8_t ch){(void)s;return ((F*)p)->visible&&ch==f->scn;}
static int qu(void *p,const Inbound *s){(void)s;return ((F*)p)->quiet;}
static int ba(void *p,const Inbound *s){(void)s;return ((F*)p)->quiet;}
static void run(unsigned ch,int should_work){
 F state={0};Inbound in={0};InboundOps ops={0};Mu1005NativeGateAdapter adp={0};
 Mu1005NativeProof proof={0};Au108OEM oem={0};Au108Port gate={0};int result;
 state.owner=state.phone=state.mfi=state.usb=state.visible=state.quiet=1;
 state.scn=ch;f=&state;
 ops.reg=rg;ops.unreg=ur;ops.add=ad;ops.remove=de;
 ops.accept=ac;ops.limit=li;ops.send=se;ops.disconnect=di;
 ops.certify=one;ops.peer_ok=one;ops.ready=one;ops.opened=op;
 ops.closed=cl;ops.rx=rx;ops.credit=cr;ops.addr=addr;
 assert(inbound_prepare(&in,&ops,0x1234)==0);
 proof.context=&state;proof.exact_image_live=ow;proof.on_original_owner_thread=ow;
 proof.paired_phone=ph;proof.mfi_hardware_ready=mf;proof.wired_idle=wi;
 proof.rf_sdp_visible=vis;proof.callbacks_quiesced=qu;proof.withdraw_barrier=ba;
 assert(mu1005_native_gate_make(&adp,&in,&proof,&oem)==0);
 au108_init(&gate);result=au108_bind(&gate,&oem,25);
 assert(state.reg==1);
 if(!should_work){assert(result==-2 && gate.state==AU108_POISONED);return;}
 assert(result==0 && in.state==IN_PUBLISHED && state.add==2 &&
        in.slot==ch && in.protocol[13]==ch && au108_live(&gate)==1);
 state.usb=0;
 assert(au108_live(&gate)==0 && gate.state==AU108_SUSPENDED);
 assert(au108_withdraw(&gate)==0 && gate.state==AU108_RETIRED);
 assert(state.del==2 && state.unreg==1);
}
int main(void){
 run(1,1);run(7,1);run(30,1);run(31,0);
 puts("PASS: original MU1005 inbound.c linked through owner gate");
 return 0;
}
