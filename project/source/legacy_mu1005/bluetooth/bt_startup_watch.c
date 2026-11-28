/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_startup_watch.h"
#include "bt_startup_snapshot.h"
int mu_bt_startup_watch_tick_source(MuBtStartupWatch *w,int(*take)(void*),void *source,unsigned ticks,
 int(*enter)(void*),void(*leave)(void*),
 int(*read_live)(void*,uint32_t,void*,size_t),void(*ready)(void*,uint32_t),void *ctx){
 if(!w||!ticks||ticks>100u||!enter||!leave||!read_live||!ready)return -1;
 /* The donor DID trace is optional. With no source, inspect the real
  * initialized service once per supervisor tick under the same lease.
  * The supervisor owns the finite overall deadline. Never synthesize
  * a trace or treat a snapshot as startup/owner authority. */
 if(take){
  int event=take(source);
  if(event!=0&&event!=1){w->remaining=0;return -1;}
  if(event==1&&!w->remaining)w->remaining=ticks;
  if(!w->remaining)return 0;
  --w->remaining;
 }else w->remaining=0;
 int acquired=enter(ctx);
 if(acquired==0)return 0;
 if(acquired!=1){w->remaining=0;return -1;}
 uint32_t service=0;
 if(mu_bt_sdk_lifecycle_sites(read_live,ctx))
     service=mu_bt_sdk_service_address(read_live,ctx);
 if(service){
     w->remaining=0;
     ready(ctx,service);
 }
 leave(ctx);
 return service?1:0;
}

static int take_latch(void*p){return mu_bt_trace_latch_take(p);}
int mu_bt_startup_watch_tick(MuBtStartupWatch *w,MuBtTraceLatch *l,unsigned ticks,
 int(*enter)(void*),void(*leave)(void*),
 int(*read_live)(void*,uint32_t,void*,size_t),void(*ready)(void*,uint32_t),void *ctx){
 if(!l)return -1;
 return mu_bt_startup_watch_tick_source(w,take_latch,l,ticks,enter,leave,read_live,ready,ctx);
}
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include "bt_queue_pulse.h"
typedef struct {int(*take)(void);int observed;const MuBtQueuePulseBinding *queue;int event;} BootStep;
static int boot_take(void*p){BootStep*s=p;s->event=s->take();return s->event;}
static int boot_enter(void*p){BootStep*s=p;return s->queue->enter(s->queue->lease_context);}
static void boot_leave(void*p){BootStep*s=p;s->queue->leave(s->queue->lease_context);}
static int boot_read(void*p,uint32_t a,void*b,size_t n){BootStep*s=p;return s->queue->owner.read_live(s->queue->owner.opaque,a,b,n);}
static void boot_ready(void*p,uint32_t service){BootStep*s=p;(void)service;s->observed=1;}
/* Supervisor worker, not OEM event thread. The source must be the already
 * attached original preload trace provider in this process. Reuse existing
 * startup watch and queue bootstrap; no retained object pointer crosses
 * the lease boundary. Queue bootstrap reacquires its own lease and checks
 * actual started object and live GOT addresses again. */
int mu_bt_startup_bootstrap_tick(MuBtStartupWatch*w,int(*trace_take)(void),unsigned ticks,
 const MuBtQueuePulseBinding*q,const MuBtNotifyBinding*n,int(*startup)(void*),void*ctx){
 if(!w||!q||!n||!startup||!q->enter||!q->leave||!q->owner.read_live)return -1;
 unsigned prior=w->remaining;
 BootStep s={trace_take,0,q,0};
 int rc=mu_bt_startup_watch_tick_source(w,trace_take?boot_take:NULL,&s,ticks,boot_enter,boot_leave,boot_read,boot_ready,&s);
 if(rc<=0)return rc;
 if(!s.observed)return -1;
 int ready=startup(ctx);
 if(ready==0){
  if(!trace_take)return 0; /* bounded by supervisor, not DID */
  /* Service visibility can precede the independent startup witness.
   * Keep only the unspent observation budget; never retry partial attach. */
  unsigned budget=prior?prior:(s.event==1?ticks:0u);
  w->remaining=budget>0u?budget-1u:0u;
  return w->remaining?0:-1;
 }
 if(ready!=1)return -1;
 return mu_bt_queue_bootstrap(q,n,startup,ctx)==0?1:-1;
}
#else
#include "bt_queue_pulse.h"
int mu_bt_startup_bootstrap_tick(MuBtStartupWatch*w,int(*trace_take)(void),unsigned ticks,
 const MuBtQueuePulseBinding*q,const MuBtNotifyBinding*n,int(*startup)(void*),void*ctx){
 (void)w;(void)trace_take;(void)ticks;(void)q;(void)n;(void)startup;(void)ctx;return -1;
}
#endif
