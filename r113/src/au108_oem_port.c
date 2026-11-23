#include "au108_oem_port.h"
#include <string.h>
static int full_owner(const Au108OEM *o){
 return o && o->exact_image_live && o->on_original_owner_thread &&
   o->get_live_iap2_scn && o->paired_phone && o->mfi_hardware_ready &&
   o->register_iap2 && o->published_iap2_service &&
   o->unregister_iap2 && o->callbacks_quiesced;
}
static int safe_owner(const Au108OEM *o){
 return full_owner(o) && o->exact_image_live(o->context)==1 &&
    o->on_original_owner_thread(o->context)==1 && o->paired_phone(o->context)==1 &&
    o->mfi_hardware_ready(o->context)==1;
}
void au108_init(Au108Port *p){if(p){memset(p,0,sizeof(*p));r105_gate_init(&p->gate);}}
int au108_bind(Au108Port *p,const Au108OEM *o,uint32_t generation,int wired_active){
 uint8_t scn=0;
 if(!p||p->state!=AU108_EMPTY||!generation||wired_active||!safe_owner(o))return -1;
 if(o->get_live_iap2_scn(o->context,&scn)!=1 || scn<1 || scn>30)return -1;
 p->owner=*o;
 p->generation=generation;
 p->evidence=(R105Evidence){generation,scn,1,1,1,1,1,0};
 if(r105_gate_arm(&p->gate,&p->evidence)!=0)return -1;
 /* If registration has an ambiguous outcome, do NOT assume it was undone. */
 p->state=AU108_POISONED;
 if(!safe_owner(&p->owner) || r105_gate_allow(&p->gate,&p->evidence)!=1)return -2;
 if(p->owner.register_iap2(p->owner.context,scn)!=0)return -2;
 /* A zero register return does not prove that iAP2 SDP was published. */
 if(!safe_owner(&p->owner) ||
    p->owner.published_iap2_service(p->owner.context,scn)!=1)return -2;
 p->state=AU108_LIVE;return 0;
}
int au108_live(Au108Port *p){
 if(!p||p->state!=AU108_LIVE)return 0;
 if(!safe_owner(&p->owner) ||
    p->owner.published_iap2_service(p->owner.context,p->evidence.scn)!=1 ||
    r105_gate_allow(&p->gate,&p->evidence)!=1){
  p->state=AU108_POISONED;r105_gate_revoke(&p->gate);return 0;
 }
 return 1;
}
int au108_withdraw(Au108Port *p){
 if(!p||p->state!=AU108_LIVE||!safe_owner(&p->owner))return -1;
 p->state=AU108_POISONED;
 if(p->owner.unregister_iap2(p->owner.context)!=0)return -2;
 if(p->owner.callbacks_quiesced(p->owner.context)!=1)return -2;
 r105_gate_revoke(&p->gate);p->state=AU108_RETIRED;return 0;
}
