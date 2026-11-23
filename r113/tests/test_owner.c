/* R113 host fixture. 9 is a SYNTHETIC test channel, never install. */
#include "au108_oem_port.h"
#include <assert.h>
#include <stdio.h>
typedef struct { int published,reg_count,stop_count,ready; } Fixture;
static int yes(void *v) { return ((Fixture*)v)->ready; }
static int scn(void *v, uint8_t *p){ (void)v; *p=9; return 1; }
static int reg(void *v,uint8_t n) { Fixture *f=(Fixture*)v; ++f->reg_count; return n==9?0:-1; }
static int pub(void *v,uint8_t n){ return ((Fixture*)v)->published && n==9; }
static int unreg(void *v){ ++((Fixture*)v)->stop_count;return 0; }
static Au108OEM owner(Fixture *f){
 Au108OEM o={0};o.context=f;o.exact_image_live=yes;o.on_original_owner_thread=yes;
 o.get_live_iap2_scn=scn;o.paired_phone=yes;o.mfi_hardware_ready=yes;
 o.register_iap2=reg;o.published_iap2_service=pub;
 o.unregister_iap2=unreg;o.callbacks_quiesced=yes;return o;
}
int main(void){
 Fixture f={0,0,0,1};Au108Port p;Au108OEM o=owner(&f);
 au108_init(&p);assert(au108_bind(&p,&o,10,1)==-1);assert(f.reg_count==0);
 au108_init(&p);assert(au108_bind(&p,&o,10,0)==-2);
 assert(f.reg_count==1 && p.state==AU108_POISONED);assert(au108_bind(&p,&o,10,0)==-1);
 f.published=1;au108_init(&p);
 assert(au108_bind(&p,&o,11,0)==0);assert(au108_live(&p)==1);
 f.published=0;assert(au108_live(&p)==0 && p.state==AU108_POISONED);
 assert(au108_withdraw(&p)==-1);
 f.published=1;au108_init(&p);
 assert(au108_bind(&p,&o,12,0)==0);
 assert(au108_withdraw(&p)==0 && f.stop_count==1);
 puts("R113 OEM service publication gate: host tests PASS; OEM owner unverified");
 return 0;
}
