#include "au108_oem_port.h"
#include <string.h>
static int all_functions(const Au108OEM *o){
 return o && o->exact_image_live && o->on_original_owner_thread &&
  o->paired_phone && o->mfi_hardware_ready && o->wired_idle &&
  o->register_iap2_rfcomm && o->publish_iap2_sdp &&
  o->published_iap2_service && o->unregister_iap2 && o->callbacks_quiesced;
}
/* Unregister may be necessary AFTER USB takeover or BT/MFi disconnection.
 * Do not require a still paired phone or an idle wired session on teardown. */
static int teardown_owner(const Au108OEM *o){
 return o && o->exact_image_live && o->on_original_owner_thread &&
  o->callbacks_quiesced && o->unregister_iap2 &&
  o->exact_image_live(o->context)==1 &&
  o->on_original_owner_thread(o->context)==1;
}
static int preflight(const Au108OEM *o){
 return all_functions(o) && o->exact_image_live(o->context)==1 &&
  o->on_original_owner_thread(o->context)==1 &&
  o->paired_phone(o->context)==1 &&
  o->mfi_hardware_ready(o->context)==1 && o->wired_idle(o->context)==1;
}
void au108_init(Au108Port *p){if(p){memset(p,0,sizeof(*p));r105_gate_init(&p->gate);}}
int au108_bind(Au108Port *p,const Au108OEM *o,uint32_t generation){
 uint8_t scn=0;int ret;
 if(!p||p->state!=AU108_EMPTY||!generation||!preflight(o))return -1;
 p->owner=*o;p->generation=generation;
 /* From here we may have changed the native SDK even if it reports failure.
  * Never auto-retry, and never free resident callback/SDP storage. */
 p->state=AU108_POISONED;
 ret=p->owner.register_iap2_rfcomm(p->owner.context,&scn);
 if(ret!=0||scn<R105_SCN_MIN||scn>R105_SCN_MAX)return -2;
 /* SCN is the SDK's RETURNED byte. Recheck owner and USB before publishing. */
 if(!preflight(&p->owner) ||
    p->owner.publish_iap2_sdp(p->owner.context,scn)!=0)return -2;
 if(!preflight(&p->owner) ||
    p->owner.published_iap2_service(p->owner.context,scn)!=1)return -2;
 p->evidence=(R105Evidence){generation,scn,1,1,1,1,1,0};
 if(r105_gate_arm(&p->gate,&p->evidence)!=0)return -2;
 if(!preflight(&p->owner)){
  r105_gate_revoke(&p->gate);return -2;
 }
 p->state=AU108_LIVE;return 0;
}
int au108_live(Au108Port *p){
 if(!p||p->state!=AU108_LIVE)return 0;
 if(!preflight(&p->owner)||
    p->owner.published_iap2_service(p->owner.context,p->evidence.scn)!=1||
    r105_gate_allow(&p->gate,&p->evidence)!=1){
  p->state=AU108_SUSPENDED;r105_gate_revoke(&p->gate);return 0;
 }
 return 1;
}
int au108_withdraw(Au108Port *p){
 if(!p||(p->state!=AU108_LIVE&&p->state!=AU108_SUSPENDED)||
    !teardown_owner(&p->owner))return -1;
 /* Do not call SDK unregistration if callbacks can still refer to the memory. */
 if(p->owner.callbacks_quiesced(p->owner.context)!=1)return -1;
 p->state=AU108_POISONED;
 if(p->owner.unregister_iap2(p->owner.context)!=0)return -2;
 if(p->owner.callbacks_quiesced(p->owner.context)!=1)return -2;
 r105_gate_revoke(&p->gate);p->state=AU108_RETIRED;return 0;
}
