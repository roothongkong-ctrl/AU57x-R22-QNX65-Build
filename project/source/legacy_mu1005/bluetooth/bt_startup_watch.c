/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_startup_watch.h"
#include "bt_startup_snapshot.h"
int mu_bt_startup_watch_tick(MuBtStartupWatch *w,MuBtTraceLatch *l,unsigned ticks,
 int(*enter)(void*),void(*leave)(void*),
 int(*read_live)(void*,uint32_t,void*,size_t),void(*ready)(void*,uint32_t),void *ctx){
 if(!w||!l||!ticks||ticks>100u||!enter||!leave||!read_live||!ready)return -1;
 /* A new trace does not indefinitely extend a pending startup window. */
 if(mu_bt_trace_latch_take(l)&&!w->remaining)w->remaining=ticks;
 if(!w->remaining)return 0;
 --w->remaining;
 if(!enter(ctx))return 0;
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
