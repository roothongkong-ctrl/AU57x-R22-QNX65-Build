/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_notify_bridge.h"
void mu_notify_call(MuNotifyGate*g,MuNotifyOriginal original,void*self,unsigned char value){
 if(!original)return;
 if(!g||!g->on_owner||g->on_owner(g->opaque)!=1||g->inside||
    !g->enabled||g->enabled(g->opaque)!=1||!g->after){
  original(self,value);return;
 }
 g->inside=1;
 original(self,value);
 if(g->on_owner(g->opaque)==1&&g->enabled(g->opaque)==1)g->after(g->opaque);
 g->inside=0;
}
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include "bt_owner_identity.h"
#include "bt_startup_snapshot.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
/* Exact original constructor stores callback self, function and zero
 * adjustment at OSAPI+0x2c/30/34. Only this writable slot is changed. */
#define OSAPI 0x2d5644u
#define SLOT (OSAPI+0x30u)
#define ORIGINAL 0x278448u
static struct {
 MuBtNotifyBinding b;
 MuBtThreadIdentity thread;
 MuNotifyGate gate;
 volatile unsigned active;
 int attempted,queue_only;
} n;
static uint32_t tid(void){return (uint32_t)(uintptr_t)pthread_self();}
static int on_owner(void*p){(void)p;return tid()==n.thread.tid;}
static int enabled(void*p){(void)p;return __sync_val_compare_and_swap(&n.active,0u,0u)==1u;}
static int safe(const MuBtNotifyBinding*b){
 return b->policy.safe_dispatch(b->policy.opaque)==1&&
 b->proof.original_image_verified(b->proof.opaque)==1&&
 b->proof.on_original_iap2_owner(b->proof.opaque)==1&&
 b->owner.owner_ready(b->owner.opaque)==1;
}
static int combined_ack_witness(void*p,int fd){
 const MuBtNotifyBinding*b=p;
 return b->daemon_verified(b->daemon_context,fd)==1&&
        b->ack_daemon_verified(b->daemon_context,fd)==1;
}
static void after(void*p){
 (void)p;
 if(!safe(&n.b)||!mu_bt_osapi_identity_matches(&n.thread,tid(),n.b.owner.read_live,n.b.owner.opaque))return;
 if(n.b.worker&&n.b.handoff->state==AU98_EMPTY&&*n.b.prepared_fd<0&&
    n.b.policy.wired_idle(n.b.policy.opaque)==1)
  (void)mu_bt_worker_poll(n.b.worker,n.b.prepared_fd);
 int rc=n.b.ack_daemon_verified?
  au98_drive_stock_native_ack(n.b.handoff,n.b.prepared_fd,&n.b.policy,&n.b.proof,&n.b.owner,combined_ack_witness,&n.b):
  au98_drive_stock_native(n.b.handoff,n.b.prepared_fd,&n.b.policy,&n.b.proof,&n.b.owner,n.b.daemon_verified,n.b.daemon_context);
 /* Failed preflight never transfers ownership. Worker sockets have
  * default non-lingering close; do not keep retrying a stale descriptor.
  * An ambiguous native attach already transferred ownership, never close. */
 if(rc==-1&&n.b.worker&&n.b.handoff->state==AU98_EMPTY&&*n.b.prepared_fd>=0){
  (void)close(*n.b.prepared_fd);*n.b.prepared_fd=-1;
 }
}
static void notify(void*self,unsigned char value){
 MuNotifyOriginal original=(MuNotifyOriginal)(uintptr_t)ORIGINAL;
 if((uintptr_t)self!=OSAPI){original(self,value);return;}
 mu_notify_call(&n.gate,original,self,value);
}
static uint32_t le(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int words(const MuBtOwner*o,uint32_t fn){
 static const uint32_t a[][2]={{0x279e28,0xe584402c},{0x279e30,0xe5845034},{0x279e38,0xe5843030},
 {0x277b30,0xe590200c},{0x277b44,0x0590c004},{0x277b4c,0x05903008},
 {0x277b50,0x008c00c2},{0x277b58,0xe12fff33},{0x278448,0xeafdb0df},
 {0x1e4b60,0xeb02565d},{0x1e4b64,0xeb02554d},{0x1e4b68,0xe28dd014},{0x1e4b6c,0xe8bd8ff0}};
 uint8_t b[12];
 for(size_t i=0;i<sizeof a/sizeof a[0];++i)
  if(o->read_live(o->opaque,a[i][0],b,4)!=1||le(b)!=a[i][1])return 0;
 return o->read_live(o->opaque,OSAPI+0x2cu,b,12)==1&&
 le(b)==OSAPI&&le(b+4)==fn&&le(b+8)==0;
}
static int bind_common(const MuBtNotifyBinding*b,int queue_only){
 MuBtThreadIdentity id;
 if(!b||n.attempted||!b->handoff||b->handoff->state!=AU98_EMPTY||!b->prepared_fd||
 !b->policy.safe_dispatch||!b->policy.wired_idle||!b->policy.withdraw_barrier||!b->policy.reclaimed||
 !b->proof.original_image_verified||!b->proof.on_original_iap2_owner||
 !b->owner.owner_ready||!b->owner.read_live||!b->owner.transport_ready||
 !b->daemon_verified||
 au100_route_select(b->proof.route_evidence)!=AU100_GRAFT_RFCOMM_ROUTE||!safe(b)||
 !mu_bt_sdk_started(b->owner.services_address,b->owner.read_live,b->owner.opaque)||
 !mu_bt_comm_words_match(b->owner.comm_load_bias,b->owner.read_live,b->owner.opaque)||
 !mu_bt_osapi_identity(b->owner.comm_load_bias,tid(),b->owner.read_live,b->owner.opaque,&id)||
 !words(&b->owner,ORIGINAL)||!safe(b))return -1;
 n.attempted=1;n.b=*b;n.thread=id;n.queue_only=queue_only;
 n.gate=(MuNotifyGate){NULL,on_owner,enabled,after,0};
 /* Publish immutable context before pointer change. No retry after a
  * competing slot writer. External safe witness must cover slot lifetime. */
 __sync_synchronize();
 if(!queue_only&&!__sync_bool_compare_and_swap((volatile uint32_t*)(uintptr_t)SLOT,ORIGINAL,(uint32_t)(uintptr_t)notify))return -1;
 __sync_lock_test_and_set(&n.active,1u);
 return 0;
}
int mu_bt_notify_bind(const MuBtNotifyBinding*b){return bind_common(b,0);}
int mu_bt_notify_bind_queue_only(const MuBtNotifyBinding*b){return bind_common(b,1);}
int mu_bt_notify_discovery_allowed(void){
 return enabled(NULL)&&on_owner(NULL)&&safe(&n.b)&&
  n.b.handoff->state==AU98_PUBLISHED&&n.b.policy.wired_idle(n.b.policy.opaque)==1;
}
int mu_bt_notify_periodic_dispatch(void){
 if(!enabled(NULL)||!on_owner(NULL)||n.gate.inside||!safe(&n.b)||
    !mu_bt_osapi_identity_matches(&n.thread,tid(),n.b.owner.read_live,n.b.owner.opaque))return -1;
 n.gate.inside=1;after(NULL);n.gate.inside=0;return 0;
}
int mu_bt_notify_unbind(void){
 if(!enabled(NULL)||!on_owner(NULL)||n.gate.inside||n.b.handoff->state!=AU98_EMPTY||
 !safe(&n.b)||!mu_bt_osapi_identity_matches(&n.thread,tid(),n.b.owner.read_live,n.b.owner.opaque)||
 !words(&n.b.owner,n.queue_only?ORIGINAL:(uint32_t)(uintptr_t)notify))return -1;
 if(!n.queue_only&&!__sync_bool_compare_and_swap((volatile uint32_t*)(uintptr_t)SLOT,(uint32_t)(uintptr_t)notify,ORIGINAL))return -1;
 __sync_lock_release(&n.active);
 /* No freeing or unloading: a previously fetched callback may still run. */
 return 0;
}
#else
int mu_bt_notify_bind(const MuBtNotifyBinding*b){(void)b;return -1;}
int mu_bt_notify_bind_queue_only(const MuBtNotifyBinding*b){(void)b;return -1;}
int mu_bt_notify_discovery_allowed(void){return 0;}
int mu_bt_notify_periodic_dispatch(void){return -1;}
int mu_bt_notify_unbind(void){return -1;}
#endif
