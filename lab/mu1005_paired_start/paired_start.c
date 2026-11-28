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
 /* A lost wired-idle admission may mean USB use or an unknown state.
  * Do not infer a USB identity or transfer its authentication. Reacquire
  * only a fresh wireless session for the uniquely selected paired ACL.
  * An independently verified idle and old job release remain mandatory.
  * This removes the need to correlate a USB record ID with a BT MAC. */
 int idle=b->ops.wired_idle_verified(s->ctx);
 if(r!=1||!gen||gen!=b->generation)s->resume_generation=0;
 else if(idle!=1&&b->state!=CP_WCP_IDLE)s->resume_generation=gen;
 if(b->state==CP_WCP_RESTORE_PENDING)return cp_wcp_bt_restore_tick(b,b->generation);
 if(r!=1||!gen||idle!=1){
  if(b->state!=CP_WCP_IDLE&&b->state!=CP_WCP_BLOCKED)
   return cp_wcp_bt_abort(b,b->generation,1);
  return CP_WCP_WAIT;
 }
 if(b->state!=CP_WCP_IDLE&&b->state!=CP_WCP_BLOCKED){
  if(gen!=b->generation)return cp_wcp_bt_abort(b,b->generation,1);
  return cp_wcp_bt_tick(b,gen,now);
 }
 if(gen==b->generation&&b->state==CP_WCP_BLOCKED&&s->resume_generation==gen){
  int rc=cp_wcp_bt_resume_wired(b,gen,now,s->released);
  if(rc==CP_WCP_OK)s->resume_generation=0;
  return rc;
 }
 if(gen<=b->generation)return CP_WCP_WAIT;
 if(b->state==CP_WCP_BLOCKED)return cp_wcp_bt_rearm(b,gen,now,s->released);
 return cp_wcp_bt_acl_up(b,gen,now);
}

int mu_paired_start_usb_ended(MuPairedStart*s,uint64_t cycle,uint64_t now,
 int(*verify)(void*,uint64_t,uint32_t)){
 uint32_t gen=0,again=0;
 if(!s||!cycle||!verify||!s->snapshot||!s->released)return CP_WCP_INVALID;
 if(cycle<=s->last_usb_cycle)return CP_WCP_STALE;
 if(s->snapshot(s->ctx,&gen)!=1||!gen||
    verify(s->ctx,cycle,gen)!=1||
    s->snapshot(s->ctx,&again)!=1||again!=gen)
  return CP_WCP_DENIED;
 struct cp_wcp_autoconnect*b=&s->bt;
 if(b->state==CP_WCP_RESTORE_PENDING)return CP_WCP_WAIT;
 int rc;
 if(b->state==CP_WCP_BLOCKED&&gen==b->generation)
  rc=cp_wcp_bt_resume_wired(b,gen,now,s->released);
 else if(b->state==CP_WCP_BLOCKED)
  rc=cp_wcp_bt_rearm(b,gen,now,s->released);
 else if(b->state==CP_WCP_IDLE)
  rc=cp_wcp_bt_acl_up(b,gen,now);
 else return CP_WCP_WAIT;
 if(rc==CP_WCP_OK){s->last_usb_cycle=cycle;s->resume_generation=0;}
 return rc;
}
