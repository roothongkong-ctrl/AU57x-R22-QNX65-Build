/* SPDX-License-Identifier: GPL-3.0-only */
#include "paired_start.h"
#include "bt_trace_latch.h"
#include <string.h>
int mu_paired_start_init(MuPairedStart*s,const struct cp_wcp_bt_ops*o,
 int(*snapshot)(void*,uint32_t*),unsigned(*take)(void*),int(*released)(void*,uint32_t)){
 if(!s||!o||!snapshot||!take||!released)return CP_WCP_INVALID;
 memset(s,0,sizeof *s);
 if(cp_wcp_bt_init(&s->bt,o)!=CP_WCP_OK)return CP_WCP_INVALID;
 s->ctx=o->ctx;s->snapshot=snapshot;s->take=take;s->released=released;
 return CP_WCP_OK;
}
int mu_paired_start_tick(MuPairedStart*s,uint64_t now){
 if(!s||!s->snapshot||!s->take||!s->released)return CP_WCP_INVALID;
 struct cp_wcp_autoconnect*b=&s->bt;
 unsigned hint=s->take(s->ctx);
 uint32_t gen=0;
 int r=s->snapshot(s->ctx,&gen);
 /* A trace is only a wakeup: never use it as evidence, nor infer a
  * disconnect when the authoritative snapshot reports the same peer. */
 if(hint&~(MU_BT_ACL_UP|MU_BT_ACL_DOWN))r=-1;
 if(b->state==CP_WCP_RESTORE_PENDING)return cp_wcp_bt_restore_tick(b,b->generation);
 if(r!=1||!gen||b->ops.wired_idle_verified(s->ctx)!=1){
  if(b->state!=CP_WCP_IDLE&&b->state!=CP_WCP_BLOCKED)
   return cp_wcp_bt_abort(b,b->generation,1);
  return CP_WCP_WAIT;
 }
 if(b->state!=CP_WCP_IDLE&&b->state!=CP_WCP_BLOCKED){
  if(gen!=b->generation)return cp_wcp_bt_abort(b,b->generation,1);
  return cp_wcp_bt_tick(b,gen,now);
 }
 if(gen<=b->generation)return CP_WCP_WAIT;
 if(b->state==CP_WCP_BLOCKED)return cp_wcp_bt_rearm(b,gen,now,s->released);
 return cp_wcp_bt_acl_up(b,gen,now);
}
