#include "au108_oem_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
 int pre,usb,paired,reg_result,allocated_scn,publish_result,published,quiesced;
 int registrations,advertisements,unregistrations,live_checks;
} Fixture;
static int pre(void *v){return ((Fixture*)v)->pre;}
static int usb(void *v){return ((Fixture*)v)->usb;}
static int paired(void *v){return ((Fixture*)v)->paired;}
static int reg(void *v,uint8_t *scn){
 Fixture *f=v;f->registrations++;*scn=(uint8_t)f->allocated_scn;return f->reg_result;
}
static int advertise(void *v,uint8_t scn){
 Fixture*f=v;f->advertisements++;assert(f->registrations==1);assert(scn==f->allocated_scn);
 return f->publish_result;
}
static int published(void *v,uint8_t scn){
 Fixture*f=v;f->live_checks++;return f->published&&scn==f->allocated_scn;
}
static int unreg(void *v){((Fixture*)v)->unregistrations++;return 0;}
static int quiesced(void *v){return ((Fixture*)v)->quiesced;}
static Au108OEM owner(Fixture *f){
 Au108OEM o;memset(&o,0,sizeof o);o.context=f;o.exact_image_live=pre;
 o.on_original_owner_thread=pre;o.paired_phone=paired;o.mfi_hardware_ready=pre;
 o.wired_idle=usb;o.register_iap2_rfcomm=reg;o.publish_iap2_sdp=advertise;
 o.published_iap2_service=published;o.unregister_iap2=unreg;
 o.callbacks_quiesced=quiesced;return o;
}
static Fixture basic(void){Fixture f={0};f.pre=1;f.usb=1;f.paired=1;f.allocated_scn=3;f.published=1;f.quiesced=1;return f;}
int main(void){
 Au108Port p;Fixture f;Au108OEM o;
 /* No paired handset at startup: service MUST be advertised before pairing. */
 f=basic();f.paired=0;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,9)==0 && p.state==AU108_ADVERTISING);
 assert(f.registrations==1 && f.advertisements==1);
 assert(p.gate.state==R105_EMPTY && p.evidence.paired_authorized==0);
 assert(au108_live(&p)==0 && p.state==AU108_ADVERTISING);
 f.paired=1;
 assert(au108_live(&p)==1 && p.gate.state==R105_ARMED);
 assert(au108_withdraw(&p)==0);
 f=basic();f.paired=0;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,9)==0 && p.state==AU108_ADVERTISING);
 assert(au108_withdraw(&p)==0 && f.unregistrations==1);
 f=basic();o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,10)==0);
 assert(p.evidence.scn==3 && f.registrations==1 && f.advertisements==1);
 assert(au108_live(&p)==1);
 assert(au108_withdraw(&p)==0 && f.unregistrations==1);
 assert(au108_bind(&p,&o,11)==-1);
 f=basic();f.usb=0;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,11)==-1 && f.registrations==0);
 f=basic();f.pre=0;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,11)==-1 && f.registrations==0);
 f=basic();f.reg_result=12;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,11)==-2 && p.state==AU108_POISONED);
 assert(f.registrations==1 && f.advertisements==0);
 assert(au108_bind(&p,&o,12)==-1 && f.registrations==1);
 f=basic();f.allocated_scn=0;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,12)==-2 && f.advertisements==0);
 f=basic();f.allocated_scn=31;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,12)==-2 && f.advertisements==0);
 f=basic();f.published=0;o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,12)==-2 && p.state==AU108_POISONED);
 assert(f.advertisements==1 && f.live_checks==1);
 f=basic();o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,13)==0);f.usb=0;
 assert(au108_live(&p)==0 && p.state==AU108_SUSPENDED);
 assert(au108_withdraw(&p)==0 && f.unregistrations==1);
 /* No phone / no MFi must not block service retirement once owner is safe. */
 f=basic();o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,15)==0);
 f.pre=0;
 assert(au108_live(&p)==0 && p.state==AU108_SUSPENDED);
 assert(au108_withdraw(&p)==-1 && f.unregistrations==0);
 f.pre=1;
 assert(au108_withdraw(&p)==0 && f.unregistrations==1);
 f=basic();o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,16)==0);f.quiesced=0;f.usb=0;
 assert(au108_live(&p)==0 && p.state==AU108_SUSPENDED);
 assert(au108_withdraw(&p)==-1 && f.unregistrations==0);
 f.quiesced=1;assert(au108_withdraw(&p)==0 && f.unregistrations==1);
 f=basic();o=owner(&f);au108_init(&p);
 assert(au108_bind(&p,&o,14)==0);f.quiesced=0;
 assert(au108_withdraw(&p)==-1 && f.unregistrations==0);
 f.quiesced=1;assert(au108_withdraw(&p)==0);
 puts("PASS: SDK returns SCN -> SDP publishes SCN -> live witness -> R105 gate");
 return 0;
}
