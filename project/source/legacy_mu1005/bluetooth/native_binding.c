#include "native_binding.h"
#include "rf_arg_experiment.h"
#include "bt_startup_snapshot.h"
#include "bt_owner_identity.h"
#include <stdint.h>
#include <string.h>
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
/* This file is intentionally not a boot hook. The external original process
 * owner must verify exact file SHA, same image mapping, initialization and
 * callback serialization before attaching. No production default owner. */
#include "native_binding_words.h"
#include <pthread.h>
typedef struct { Mu38PeerOps peer; MuBtOwner owner; InboundR5 bridge; MuBtThreadIdentity thread; int attempted; } Resident;
static Resident res;
static uint32_t current_tid(void){return (uint32_t)(uintptr_t)pthread_self();}
static int sdk_started(const MuBtOwner *o){return mu_bt_sdk_started(o->services_address,o->read_live,o->opaque);}
static int cert(void *p){Resident *r=p;return r==&res&&r->attempted&&r->owner.owner_ready&&r->owner.owner_ready(r->owner.opaque)==1&&
 mu_bt_osapi_identity_matches(&r->thread,current_tid(),r->owner.read_live,r->owner.opaque)&&sdk_started(&r->owner);}
static int ready(void *p){Resident *r=p;return cert(p)&&r->owner.transport_ready(r->owner.opaque)==1;}
static uint32_t addr(const void *p){return (uint32_t)(uintptr_t)p;}
static int assoc(void *p,const void *ch,uint32_t a,unsigned code){
 Resident*r=p;if(!cert(r)||ch!=&r->bridge.service.rf||(code!=1&&code!=2)||!a)return 0;
 /* Read the same original registry under the independently certified
  * owner/lifetime contract. Avoid reacquiring SDK mutex in this callback.
  * The independent paired_authorized provider remains mandatory. */
 return mu38_registry_association((uint32_t)(uintptr_t)ch,a,r->owner.read_live,r->owner.opaque);
}
static int rd(void*p,uint32_t a,void*out,size_t n){Resident*r=p;return cert(r)&&r->owner.read_live(r->owner.opaque,a,out,n)==1;}
static int auth(void*p,uint32_t a,const uint8_t mac[6]){Resident*r=p;return cert(r)&&r->owner.paired_authorized(r->owner.opaque,a,mac)==1;}
/* MU1005 arg3 is initial RX credit budget; allocated SCN is *slot. */
static int register_verified(void *channel,uint8_t *slot,uint16_t ignored){
 Resident *r=&res;
 (void)ignored;
 if(!ready(r)||r->owner.rf_arg3_snapshot_valid!=1u||
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
static int checked_word(const MuBtOwner*o,uint32_t a,uint32_t want){
 uint8_t b[4];
 if(o->read_live(o->opaque,a,b,4)!=1)return 0;
 return ((uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24)==want;
}
static int words_ok(const MuBtOwner*o){
 for(size_t i=0;i<sizeof mu_bt_words/sizeof mu_bt_words[0];i++)
  if(!checked_word(o,mu_bt_words[i][0],mu_bt_words[i][1]))return 0;
 return 1;
}
/* Original SDP add/remove already schedule refresh. Reissue after full
 * publication so the asynchronous EIR builder can observe final readiness.
 * Does not insert UUID or claim controller completion. */
static int eir_request_words(const MuBtOwner*o){
 static const uint32_t a[][2]={{0x1db4c8,0xe92d4010},{0x1db4d0,0xeb027b15},
 {0x1db4f0,0xe7831002},{0x1db500,0xe7c31002},{0x1db510,0xeb0025f7},{0x1db518,0xea027ae0}};
 for(size_t i=0;i<sizeof a/sizeof a[0];i++)
  if(!checked_word(o,a[i][0],a[i][1]))return 0;
 return 1;
}
int mu_bt_native_refresh_eir(void){
 if(!cert(&res)||!eir_request_words(&res.owner)||!cert(&res))return -1;
 ((void(*)(void))(uintptr_t)0x1db4c8u)();
 return 0;
}
#define RFN(type,a) ((type)(uintptr_t)(a))
int mu_bt_native_attach(const MuBtOwner *o,int fd){
 if(!o||!o->owner_ready||!o->transport_ready||!o->read_live||!o->paired_authorized||
    o->rf_arg3_snapshot_valid!=1u||o->rf_arg3_snapshot<1u||o->rf_arg3_snapshot>7u||
    res.attempted||fd<0||o->owner_ready(o->opaque)!=1||o->transport_ready(o->opaque)!=1)return -1;
 MuBtThreadIdentity identity;
 if(!words_ok(o)||!eir_request_words(o)||!sdk_started(o)||!mu_bt_comm_words_match(o->comm_load_bias,o->read_live,o->opaque)||
    !mu_bt_osapi_identity(o->comm_load_bias,current_tid(),o->read_live,o->opaque,&identity)||
    o->owner_ready(o->opaque)!=1)return -1;
 res.attempted=1;res.owner=*o;res.thread=identity;res.peer=(Mu38PeerOps){&res,assoc,rd,auth};
 InboundOps n={0};n.reg=register_verified;n.unreg=RFN(int (*)(void*,uint8_t*),0x2779b4u);
 n.add=RFN(int (*)(void*),0x207244u);n.remove=RFN(int (*)(void*),0x2071c4u);
 n.accept=RFN(int (*)(void*,int),0x27733cu);n.limit=RFN(int (*)(void*),0x277074u);
 n.send=RFN(int (*)(void*,void*),0x2771f8u);n.disconnect=RFN(int (*)(void*),0x2773f8u);n.credit=RFN(int (*)(void*,int),0x27715cu);
 n.certify=cert;n.ready=ready;n.addr=addr;n.opaque=&res;
 n.initial_rx_credits=res.owner.rf_arg3_snapshot;
 if(ir5_init(&res.bridge,&n,mu38_resolve_peer,fd,(uint32_t)(uintptr_t)callback))return -1;
 int rc=ir5_publish(&res.bridge);
 if(rc==0)(void)mu_bt_native_refresh_eir();
 return rc;
}
int mu_bt_native_discovery_ready(void){return ir5_discovery_ready(&res.bridge);}
int mu_bt_native_eir_buffer_matches(const uint8_t*b){
 return cert(&res)&&res.owner.services_address<=UINT32_MAX-0x40cu&&
  (uintptr_t)b==(uintptr_t)(res.owner.services_address+0x31cu)&&
  ir5_discovery_ready(&res.bridge)==1;
}
int mu_bt_native_tick(void){return ready(&res)&&res.bridge.initialized?ir5_tick(&res.bridge):-1;}
int mu_bt_native_withdraw(int (*barrier)(void*,const Inbound*),void *p){
 if(!cert(&res)||!res.bridge.initialized||!barrier)return -1;
 int r=ir5_drain(&res.bridge);
 if(r)return r;
 r=ir5_withdraw(&res.bridge,barrier,p);
 if(r==0)(void)mu_bt_native_refresh_eir();
 return r;
}
/* A retired SDK object may still have queued callbacks. Require a separate
 * post-withdraw reclamation witness, not the prior unregister barrier. */
int mu_bt_native_reclaim(int (*reclaimed)(void*,const Inbound*),void*p){
 InboundR5*b=&res.bridge;
 if(!cert(&res)||!reclaimed||!b->initialized||!b->dead||
    b->service.state!=IN_RETIRED||b->service.published_mask||
    b->service.uncertain_mask||b->service.pending||b->service.busy||
    b->native_link||b->native_rx_busy||b->completion||b->rf.tx_busy||
    reclaimed(p,&b->service)!=1||!cert(&res))return -1;
 memset(&res,0,sizeof res);return 0;
}
#else
int mu_bt_native_attach(const MuBtOwner *o,int fd){(void)o;(void)fd;return -1;}
int mu_bt_native_discovery_ready(void){return 0;}
int mu_bt_native_eir_buffer_matches(const uint8_t*b){(void)b;return 0;}
int mu_bt_native_refresh_eir(void){return -1;}
int mu_bt_native_tick(void){return -1;}
int mu_bt_native_withdraw(int (*barrier)(void*,const Inbound*),void *p){(void)barrier;(void)p;return -1;}
int mu_bt_native_reclaim(int (*barrier)(void*,const Inbound*),void *p){(void)barrier;(void)p;return -1;}
#endif
