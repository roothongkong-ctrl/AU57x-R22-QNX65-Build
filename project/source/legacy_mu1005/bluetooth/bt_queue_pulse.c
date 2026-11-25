/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "bt_queue_pulse.h"
static unsigned load(volatile unsigned*p){return __sync_val_compare_and_swap(p,0u,0u);}
int mu_bt_pulse_request(MuBtPulse*p){
 if(!p||load(&p->active)!=1u||!__sync_bool_compare_and_swap(&p->pending,0u,1u))return 0;
 if(load(&p->active)!=1u){mu_bt_pulse_release(p);return 0;}return 1;
}
int mu_bt_pulse_execute(MuBtPulse*p,int(*run)(void*),void*ctx){
 if(!p||!run||!__sync_bool_compare_and_swap(&p->pending,1u,2u))return 0;
 return load(&p->active)==1u&&run(ctx)==1?1:0;
}
void mu_bt_pulse_release(MuBtPulse*p){if(p)__sync_lock_release(&p->pending);}
void mu_bt_pulse_disable(MuBtPulse*p){if(p)__sync_lock_release(&p->active);}
int mu_bt_pulse_idle(MuBtPulse*p){return p&&load(&p->pending)==0;}
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include "bt_owner_identity.h"
#include "bt_startup_snapshot.h"
#include <time.h>
/* Native util::SharedPtrDefaultBase is three words, not two. */
typedef struct {uint32_t control,object,counter;} Share;
typedef void(*Construct)(Share*,void*,void(*)(void*,void*),void*);
typedef void(*Copy)(Share*,const Share*);
typedef void(*Destroy)(Share*);
typedef void(*Enque)(void*,const Share*);
typedef struct {void(*d1)(void*);void(*d0)(void*);int(*run)(void*);int(*matches)(void*,const void*);} Vtable;
typedef struct {const Vtable*vptr;} Event;
static struct {
 MuBtPulse pulse;MuBtQueuePulseBinding b;MuBtThreadIdentity thread;Share ao;
 Construct construct;Copy copy;Destroy destroy;Enque enque;pthread_t worker;
 MuBtNotifyBinding bootstrap;
 int attempted,started,stopped,held,valid_share,bootstrap_needed;
} q;
static uint32_t tid(void){return (uint32_t)(uintptr_t)pthread_self();}
static int dispatch(void*p){
 (void)p;
 /* Never treat the startup thread's authority as event thread authority. */
 if(tid()!=q.thread.tid||!q.b.owner.owner_ready||
    q.b.owner.owner_ready(q.b.owner.opaque)!=1||
    !mu_bt_osapi_identity_matches(&q.thread,tid(),q.b.owner.read_live,q.b.owner.opaque))return 0;
 if(q.bootstrap_needed){
  if(mu_bt_notify_bind_queue_only(&q.bootstrap)!=0)return 0;
  q.bootstrap_needed=0;
 }
 return mu_bt_notify_periodic_dispatch()==0;
}
static int run(void*p){(void)p;(void)mu_bt_pulse_execute(&q.pulse,dispatch,NULL);return 0;}
static int matches(void*p,const void*other){(void)p;(void)other;return 0;}
static void event_dtor(void*p){(void)p;}
static const Vtable vt={event_dtor,event_dtor,run,matches};
static const Event event={&vt};
static void released(void*obj,void*ctx){if(obj==(void*)&event&&ctx==&q)mu_bt_pulse_release(&q.pulse);}
static uint32_t le(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int word(const MuBtOwner*o,uint32_t a,uint32_t expected){
 uint8_t b[4];return o->read_live(o->opaque,a,b,4)==1&&le(b)==expected;
}
static int resolve(const MuBtQueuePulseBinding*b){
 const MuBtOwner*o=&b->owner;uint32_t u=b->util_load_bias,c=o->comm_load_bias;
 static const uint32_t us[][2]={{0x6f5f4,0xe92d4010},{0x6f5fc,0xebfffce0},{0x6e3f0,0xe5913008},{0x6fc5c,0xe92d4010},{0x6e9c0,0xe5813008},{0x6e9c4,0xe5813000},{0x6e9c8,0xe5813004},{0x6ea5c,0xe5830000},{0x6ea64,0xe583c004}};
 static const uint32_t cs[][2]={{0x4e620,0xe59030f0},{0x4e694,0xebff1b32},{0x4e6b8,0xebff1c67},{0x4e6c8,0xebff1afe},{0x4ea74,0xebff1b78},{0x4ea94,0xe593300c},{0x4eab0,0xe5933008},{0x4ed30,0xe593300c},{0x4d828,0xe5913004},{0x4d838,0xe5933008},{0x4d83c,0xe12fff33},{0x4eadc,0xebff1921}};
 if(!u||(u&0xfffu)||u>UINT32_MAX-0x6fc5cu||!c||(c&0xfffu)||c>UINT32_MAX-0xc69b4u)return 0;
 for(size_t i=0;i<sizeof us/sizeof us[0];i++)if(!word(o,u+us[i][0],us[i][1]))return 0;
 for(size_t i=0;i<sizeof cs/sizeof cs[0];i++)if(!word(o,c+cs[i][0],cs[i][1]))return 0;
 return word(o,0x2c14c4,u+0x6f5f4)&&word(o,0x2c14d4,u+0x6e3f0)&&
 word(o,0x2c13bc,u+0x6fc5c)&&word(o,0x2c165c,c+0x4e620)&&word(o,c+0xc69b4,c+0x4d828);
}
static void*producer(void*p){
 (void)p;struct timespec pause={0,20000000L};
 while(load(&q.pulse.active)==1u){
  if(mu_bt_pulse_request(&q.pulse)){
   if(q.b.enter(q.b.lease_context)!=1)mu_bt_pulse_release(&q.pulse);
   else {
    Share s={0};q.construct(&s,(void*)&event,released,&q);
    if(!s.control||!s.counter||s.object!=(uint32_t)(uintptr_t)&event)
     mu_bt_pulse_disable(&q.pulse); /* uncertain allocation: no reuse */
    else {q.enque((void*)(uintptr_t)q.ao.object,&s);q.destroy(&s);}
    q.b.leave(q.b.lease_context);
   }
  }
  (void)nanosleep(&pause,NULL);
 }
 return NULL;
}
static int begin(const MuBtQueuePulseBinding*b,const MuBtNotifyBinding*boot,int(*startup)(void*),void*ctx){
 MuBtThreadIdentity id;uint8_t f[16];const MuBtOwner*o;
 if(!b||q.attempted||!b->enter||!b->leave)return -1;
 o=&b->owner;
 if(!o->owner_ready||!o->read_live)return -1;
 if(boot&&(!startup||startup(ctx)!=1||!boot->handoff||!boot->prepared_fd||
    boot->owner.opaque!=o->opaque||boot->owner.read_live!=o->read_live||
    boot->owner.owner_ready!=o->owner_ready||boot->owner.services_address!=o->services_address||
    boot->owner.comm_load_bias!=o->comm_load_bias||
    !boot->proof.original_image_verified||boot->proof.original_image_verified(boot->proof.opaque)!=1||
    au100_route_select(boot->proof.route_evidence)!=AU100_GRAFT_RFCOMM_ROUTE))return -1;
 if(!boot&&o->owner_ready(o->opaque)!=1)return -1;
 if(b->enter(b->lease_context)!=1)return -1;
 if(!resolve(b)||!mu_bt_sdk_started(o->services_address,o->read_live,o->opaque)||
    !(boot?mu_bt_osapi_target(o->comm_load_bias,o->read_live,o->opaque,&id):
      mu_bt_osapi_identity(o->comm_load_bias,tid(),o->read_live,o->opaque,&id))||
    o->read_live(o->opaque,id.factory,f,sizeof f)!=1||le(f+4)!=id.shared_control||
    le(f+8)!=id.active_object||!le(f+12)||
    !mu_bt_osapi_identity_matches(&id,id.tid,o->read_live,o->opaque)||
    (boot?startup(ctx)!=1:o->owner_ready(o->opaque)!=1)){
  b->leave(b->lease_context);return -1;
 }
 q.attempted=1;q.b=*b;q.thread=id;
 if(boot){q.bootstrap=*boot;q.bootstrap_needed=1;}
 q.construct=(Construct)(uintptr_t)(b->util_load_bias+0x6f5f4u);
 q.copy=(Copy)(uintptr_t)(b->util_load_bias+0x6e3f0u);
 q.destroy=(Destroy)(uintptr_t)(b->util_load_bias+0x6fc5cu);
 q.enque=(Enque)(uintptr_t)(o->comm_load_bias+0x4e620u);
 q.copy(&q.ao,(const Share*)(uintptr_t)(id.factory+4u));q.held=1;
 if(q.ao.control!=le(f+4)||q.ao.object!=id.active_object||q.ao.counter!=le(f+12)){
  b->leave(b->lease_context);return -2;
 }
 q.valid_share=1;b->leave(b->lease_context);
 __sync_lock_test_and_set(&q.pulse.active,1u);
 if(pthread_create(&q.worker,NULL,producer,NULL)!=0){mu_bt_pulse_disable(&q.pulse);q.stopped=1;return -1;}
 q.started=1;return 0;
}
int mu_bt_queue_pulse_start(const MuBtQueuePulseBinding*b){return begin(b,NULL,NULL,NULL);}
int mu_bt_queue_bootstrap(const MuBtQueuePulseBinding*b,const MuBtNotifyBinding*boot,int(*startup)(void*),void*ctx){
 if(!boot||!startup)return -1;
 return begin(b,boot,startup,ctx);
}
int mu_bt_queue_pulse_stop(void){
 if(!q.started||tid()==q.thread.tid)return -1;
 if(!q.stopped){mu_bt_pulse_disable(&q.pulse);if(pthread_join(q.worker,NULL)!=0)return -1;q.stopped=1;}
 return mu_bt_pulse_idle(&q.pulse)?0:-1;
}
int mu_bt_queue_pulse_retire(void){
 if(!q.held||!q.valid_share||!q.stopped||!mu_bt_pulse_idle(&q.pulse)||tid()!=q.thread.tid||
    q.b.owner.owner_ready(q.b.owner.opaque)!=1||q.b.enter(q.b.lease_context)!=1)return -1;
 if(!mu_bt_osapi_identity_matches(&q.thread,tid(),q.b.owner.read_live,q.b.owner.opaque)){
  q.b.leave(q.b.lease_context);return -1;}
 q.destroy(&q.ao);q.held=0;q.b.leave(q.b.lease_context);return 0;
}
#else
int mu_bt_queue_pulse_start(const MuBtQueuePulseBinding*b){(void)b;return -1;}
int mu_bt_queue_bootstrap(const MuBtQueuePulseBinding*b,const MuBtNotifyBinding*n,int(*s)(void*),void*c){(void)b;(void)n;(void)s;(void)c;return -1;}
int mu_bt_queue_pulse_stop(void){return -1;}
int mu_bt_queue_pulse_retire(void){return -1;}
#endif
