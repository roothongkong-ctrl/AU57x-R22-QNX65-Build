#include "mu1005_native_gate_adapter.h"
#include <string.h>
static int exact(void *v){Mu1005NativeGateAdapter *a=v;return a->proof.exact_image_live(a->proof.context);}
static int owner(void *v){Mu1005NativeGateAdapter *a=v;return a->proof.on_original_owner_thread(a->proof.context);}
static int paired(void *v){Mu1005NativeGateAdapter *a=v;return a->proof.paired_phone(a->proof.context);}
static int mfi(void *v){Mu1005NativeGateAdapter *a=v;return a->proof.mfi_hardware_ready(a->proof.context);}
static int idle(void *v){Mu1005NativeGateAdapter *a=v;return a->proof.wired_idle(a->proof.context);}
static int register_rf(void *v,uint8_t *scn){
 Mu1005NativeGateAdapter *a=v;int rc;
 if(!scn||a->service->state!=IN_PREPARED)return -1;
 *scn=0;rc=inbound_register_rf(a->service);
 if(rc!=0)return rc;
 *scn=a->service->slot;return 0;
}
static int publish_sdp(void *v,uint8_t scn){
 Mu1005NativeGateAdapter *a=v;
 if(a->service->state!=IN_REGISTERED||a->service->slot!=scn||
    a->service->protocol[13]!=scn)return -1;
 return inbound_publish_sdp(a->service);
}
static int visible(void *v,uint8_t scn){
 Mu1005NativeGateAdapter *a=v;Inbound *s=a->service;
 if(s->state!=IN_PUBLISHED||s->slot!=scn||s->protocol[13]!=scn||
    s->uncertain_mask||s->published_mask!=(IN_RF|IN_IAP_SDP|IN_CP_SDP))return 0;
 return a->proof.rf_sdp_visible(a->proof.context,s,scn)==1;
}
static int quiet(void *v){
 Mu1005NativeGateAdapter *a=v;Inbound *s=a->service;
 if(s->pending||s->busy||s->uncertain_mask)return 0;
 /* Same owner barrier must remain valid after the real inbound_withdraw()
  * changes state to RETIRED and clears all native registrations. */
 if(s->state==IN_PUBLISHED){
  if(s->published_mask!=(IN_RF|IN_IAP_SDP|IN_CP_SDP))return 0;
 }else if(s->state==IN_RETIRED){
  if(s->published_mask!=0)return 0;
 }else return 0;
 return a->proof.callbacks_quiesced(a->proof.context,s)==1;
}
static int unregister_rf(void *v){
 Mu1005NativeGateAdapter *a=v;
 return inbound_withdraw(a->service,a->proof.withdraw_barrier,a->proof.context);
}
int mu1005_native_gate_make(Mu1005NativeGateAdapter *a,Inbound *s,
                            const Mu1005NativeProof *p,Au108OEM *out){
 if(!a||!s||!p||!out||s->state!=IN_PREPARED||
    !p->exact_image_live||!p->on_original_owner_thread||
    !p->paired_phone||!p->mfi_hardware_ready||!p->wired_idle||
    !p->rf_sdp_visible||!p->callbacks_quiesced||!p->withdraw_barrier)return -1;
 memset(a,0,sizeof(*a));a->service=s;a->proof=*p;
 memset(out,0,sizeof(*out));out->context=a;
 out->exact_image_live=exact;out->on_original_owner_thread=owner;
 out->paired_phone=paired;out->mfi_hardware_ready=mfi;out->wired_idle=idle;
 out->register_iap2_rfcomm=register_rf;out->publish_iap2_sdp=publish_sdp;
 out->published_iap2_service=visible;out->unregister_iap2=unregister_rf;
 out->callbacks_quiesced=quiet;return 0;
}
