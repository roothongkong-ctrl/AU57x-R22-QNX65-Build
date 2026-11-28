/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "stock_reconnect.h"
#include "bt_notify_bridge.h"
#include <time.h>
int mu_stock_reconnect_step(MuPairedStart*s,int ready,uint64_t now){
 if(!s||!s->snapshot||!s->bt.ops.wired_idle_verified)return CP_WCP_INVALID;
 struct cp_wcp_autoconnect*b=&s->bt;
 /* Always service original USB priority and any outstanding cancel. */
 if(b->state==CP_WCP_RESTORE_PENDING||
    b->ops.wired_idle_verified(s->ctx)!=1)
  return mu_paired_start_tick(s,now);
 if(ready!=1){
  if(b->state!=CP_WCP_IDLE&&b->state!=CP_WCP_BLOCKED)
   return cp_wcp_bt_abort(b,b->generation,1);
  return CP_WCP_WAIT;
 }
 /* No stock ACL connect call: wait for the existing paired phone.
  * Only then can the original iAP2 client workflow run. */
 return mu_paired_start_tick(s,now);
}
static void dispatch(void*v,int ready){
 struct timespec t;
 if(clock_gettime(CLOCK_MONOTONIC,&t)!=0||t.tv_sec<0||
    (uint64_t)t.tv_sec>UINT64_MAX/1000u-1u)return;
 (void)mu_stock_reconnect_step(v,ready,(uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u);
}
int mu_stock_reconnect_bind(MuPairedStart*s){
 if(!s||!s->snapshot||!s->take||!s->released)return -1;
 return mu_bt_notify_set_reconnect(dispatch,s);
}
