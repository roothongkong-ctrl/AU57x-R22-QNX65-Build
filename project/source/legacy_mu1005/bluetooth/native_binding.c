#include "native_binding.h"
#include "rf_arg_experiment.h"
#include <stdint.h>
#include <string.h>
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
/* This file is intentionally not a boot hook. The external original process
 * owner must verify exact file SHA, same image mapping, initialization and
 * callback serialization before attaching. No production default owner. */
#include "native_binding_words.h"
typedef struct { Mu38PeerOps peer; MuBtOwner owner; InboundR5 bridge; int attempted; } Resident;
static Resident res;
static int cert(void *p){Resident *r=p;return r==&res&&r->attempted&&r->owner.owner_ready&&r->owner.owner_ready(r->owner.opaque)==1;}
static int ready(void *p){Resident *r=p;return cert(p)&&r->owner.transport_ready(r->owner.opaque)==1;}
static uint32_t addr(const void *p){return (uint32_t)(uintptr_t)p;}
static int assoc(void *p,const void *ch,uint32_t a,unsigned code){
 Resident*r=p;if(!cert(r)||ch!=&r->bridge.service.rf||(code!=1&&code!=2)||!a)return 0;
 /* Native SDK association lookup, used by stock HFP callback on other event
  * types. Owner must certify lock/re-entry for this particular event path. */
 return (uintptr_t)((void *(*)(const void *))(uintptr_t)0x276f74u)(ch)==a;
}
static int rd(void*p,uint32_t a,void*out,size_t n){Resident*r=p;return cert(r)&&r->owner.read_live(r->owner.opaque,a,out,n)==1;}
static int auth(void*p,uint32_t a,const uint8_t mac[6]){Resident*r=p;return cert(r)&&r->owner.paired_authorized(r->owner.opaque,a,mac)==1;}
/* MU1005 arg3 is initial RX credit budget; allocated SCN is *slot. */
static int register_verified(void *channel,uint8_t *slot,uint16_t ignored){
 Resident *r=&res;
 (void)ignored;
 if(!cert(r)||r->owner.rf_arg3_snapshot_valid!=1u||
    r->owner.rf_arg3_snapshot<1u||r->owner.rf_arg3_snapshot>7u)return -1;
 /* Do not call the external proof function a second time after RFCOMM/TCP
  * setup: its answer could change between check and native registration. */
 return ((int (*)(void*,uint8_t*,uint16_t))(uintptr_t)0x277a18u)
     (channel,slot,r->owner.rf_arg3_snapshot);
}
static void callback(void *ch,const void *event){
 if(cert(&res)&&res.bridge.initialized&&ch==&res.bridge.service.rf)
  (void)mu38_dispatch(&res.bridge,&res.peer,ch,event);
}
/* Require native executable words at all vendor call entry points, in
 * addition to the caller's full mapped-image verification. This is not a
 * replacement for that verification. Never read an arbitrary process. */
static int words_ok(void){
 for(size_t i=0;i<sizeof mu_bt_words/sizeof mu_bt_words[0];i++){
  const volatile uint32_t *p=(const volatile uint32_t *)(uintptr_t)mu_bt_words[i][0];
  if(*p!=mu_bt_words[i][1])return 0;
 }return 1;
}
#define RFN(type,a) ((type)(uintptr_t)(a))
int mu_bt_native_attach(const MuBtOwner *o,int fd){
 if(!o||!o->owner_ready||!o->transport_ready||!o->read_live||!o->paired_authorized||
    o->rf_arg3_snapshot_valid!=1u||o->rf_arg3_snapshot<1u||o->rf_arg3_snapshot>7u||
    res.attempted||fd<0||o->owner_ready(o->opaque)!=1||o->transport_ready(o->opaque)!=1)return -1;
 if(!words_ok())return -1;
 res.attempted=1;res.owner=*o;res.peer=(Mu38PeerOps){&res,assoc,rd,auth};
 InboundOps n={0};n.reg=register_verified;n.unreg=RFN(int (*)(void*,uint8_t*),0x2779b4u);
 n.add=RFN(int (*)(void*),0x207244u);n.remove=RFN(int (*)(void*),0x2071c4u);
 n.accept=RFN(int (*)(void*,int),0x27733cu);n.limit=RFN(int (*)(void*),0x277074u);
 n.send=RFN(int (*)(void*,void*),0x2771f8u);n.disconnect=RFN(int (*)(void*),0x2773f8u);n.credit=RFN(int (*)(void*,int),0x27715cu);
 n.certify=cert;n.ready=ready;n.addr=addr;n.opaque=&res;
 n.initial_rx_credits=res.owner.rf_arg3_snapshot;
 if(ir5_init(&res.bridge,&n,mu38_resolve_peer,fd,(uint32_t)(uintptr_t)callback))return -1;
 return ir5_publish(&res.bridge);
}
int mu_bt_native_discovery_ready(void){return ir5_discovery_ready(&res.bridge);}
int mu_bt_native_tick(void){return cert(&res)&&res.bridge.initialized?ir5_tick(&res.bridge):-1;}
int mu_bt_native_withdraw(int (*barrier)(void*,const Inbound*),void *p){return cert(&res)&&res.bridge.initialized?ir5_withdraw(&res.bridge,barrier,p):-1;}
#else
int mu_bt_native_attach(const MuBtOwner *o,int fd){(void)o;(void)fd;return -1;}
int mu_bt_native_discovery_ready(void){return 0;}
int mu_bt_native_tick(void){return -1;}
int mu_bt_native_withdraw(int (*barrier)(void*,const Inbound*),void *p){(void)barrier;(void)p;return -1;}
#endif
