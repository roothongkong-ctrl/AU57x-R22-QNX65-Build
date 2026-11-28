/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_wcp_autoconnect.h"
#include <string.h>

int cp_wcp_stock_client_result(int raw,int no_request_proven){
 if(raw==2)return CP_WCP_OPEN_PENDING;
 return no_request_proven==1?CP_WCP_OPEN_REJECTED:CP_WCP_OPEN_UNCERTAIN;
}
static int owner(const struct cp_wcp_autoconnect *s){
 return s->ops.stock_owner_verified(s->ops.ctx,s->generation)==1;
}
static int idle(const struct cp_wcp_autoconnect *s){
 return s->ops.wired_idle_verified(s->ops.ctx)==1;
}
static int deadline(uint64_t now,uint64_t delay,uint64_t *out){
 if(now>UINT64_MAX-delay)return 0;
 *out=now+delay;return 1;
}
int cp_wcp_bt_init(struct cp_wcp_autoconnect *s,const struct cp_wcp_bt_ops *ops){
 if(!s||!ops||!ops->stock_owner_verified||!ops->paired_acl_verified||
    !ops->wired_idle_verified||
    !ops->stock_iap2_connected||!ops->phone_iap2_sdp||
    !ops->stock_open_iap2||!ops->stock_cancel_iap2)return CP_WCP_INVALID;
 memset(s,0,sizeof *s);s->ops=*ops;return CP_WCP_OK;
}
static int begin_acl(struct cp_wcp_autoconnect *s,uint32_t gen,uint64_t now){
 if(s->generation&&gen<=s->generation)return CP_WCP_STALE;
 if(s->generation&&now<s->last_ms)return CP_WCP_CLOCK;
 if(!s->ops.stock_owner_verified||!s->ops.wired_idle_verified||
    s->ops.stock_owner_verified(s->ops.ctx,gen)!=1||
    s->ops.paired_acl_verified(s->ops.ctx,gen)!=1||
    s->ops.wired_idle_verified(s->ops.ctx)!=1)return CP_WCP_DENIED;
 if(!deadline(now,CP_WCP_INITIAL_MS,&s->deadline_ms))return CP_WCP_CLOCK;
 s->generation=gen;s->last_ms=now;s->state=CP_WCP_DELAY;
 s->attempts=0;s->requested=0;
 return CP_WCP_OK;
}
int cp_wcp_bt_acl_up(struct cp_wcp_autoconnect *s,uint32_t gen,uint64_t now){
 if(!s||!gen)return CP_WCP_INVALID;
 if(s->state!=CP_WCP_IDLE||s->requested)return CP_WCP_DENIED;
 return begin_acl(s,gen,now);
}
int cp_wcp_bt_rearm(struct cp_wcp_autoconnect *s,uint32_t gen,uint64_t now,
 int (*old_release_verified)(void *,uint32_t)){
 if(!s||!gen||!old_release_verified)return CP_WCP_INVALID;
 if(s->state!=CP_WCP_BLOCKED||s->requested)return CP_WCP_DENIED;
 if(gen<=s->generation)return CP_WCP_STALE;
 if(now<s->last_ms)return CP_WCP_CLOCK;
 if(old_release_verified(s->ops.ctx,s->generation)!=1)return CP_WCP_DENIED;
 return begin_acl(s,gen,now);
}
/* Never silently forget an outstanding native open. If cancel cannot be
 * proven, do not advertise IDLE or let a new generation take the channel. */
int cp_wcp_bt_abort(struct cp_wcp_autoconnect *s,uint32_t gen,int wired){
 if(!s||!gen)return CP_WCP_INVALID;
 if(s->state==CP_WCP_IDLE)return CP_WCP_STALE;
 if(gen!=s->generation)return CP_WCP_STALE;
 if(s->state==CP_WCP_BLOCKED)return wired?CP_WCP_OK:CP_WCP_DENIED;
 if(s->requested){
  if(!owner(s)||s->ops.stock_cancel_iap2(s->ops.ctx,gen)!=1){
   s->state=CP_WCP_RESTORE_PENDING;
   return CP_WCP_WAIT;
  }
  s->requested=0;
 }
 s->state=wired?CP_WCP_BLOCKED:CP_WCP_IDLE;
 return CP_WCP_OK;
}
int cp_wcp_bt_restore_tick(struct cp_wcp_autoconnect *s,uint32_t gen){
 if(!s||!gen)return CP_WCP_INVALID;
 if(gen!=s->generation||s->state!=CP_WCP_RESTORE_PENDING)
  return CP_WCP_STALE;
 if(!owner(s)||!s->requested||
    s->ops.stock_cancel_iap2(s->ops.ctx,gen)!=1)return CP_WCP_WAIT;
 s->requested=0;
 /* A failed or interrupted teardown is never followed by an automatic
  * reconnect: a fresh original ACL event and a fresh generation are needed. */
 s->state=CP_WCP_BLOCKED;
 return CP_WCP_OK;
}
int cp_wcp_bt_tick(struct cp_wcp_autoconnect *s,uint32_t gen,uint64_t now){
 int peer,rc;
 if(!s||!gen)return CP_WCP_INVALID;
 if(s->state==CP_WCP_IDLE||gen!=s->generation)return CP_WCP_STALE;
 if(now<s->last_ms)return CP_WCP_CLOCK;
 s->last_ms=now;
 if(s->state==CP_WCP_RESTORE_PENDING)return CP_WCP_WAIT;
 if(s->state==CP_WCP_BLOCKED)return CP_WCP_DENIED;
 if(!owner(s)||!idle(s)||
    s->ops.paired_acl_verified(s->ops.ctx,gen)!=1){
  (void)cp_wcp_bt_abort(s,gen,1);
  return CP_WCP_DENIED;
 }
 if(s->ops.stock_iap2_connected(s->ops.ctx,gen)==1){
  /* A real BlueSDK connected result proves only transport; the existing
   * R44 iAP2/MFi authentication MUST still run before 0x4301 is allowed. */
  s->state=CP_WCP_READY;
  return CP_WCP_LINK_READY;
 }
 if(s->state==CP_WCP_READY){
  (void)cp_wcp_bt_abort(s,gen,1);return CP_WCP_DENIED;
 }
 if(now<s->deadline_ms)return CP_WCP_WAIT;
 if(s->state==CP_WCP_OPENING){
  /* Never retry while a vendor open might still be outstanding. */
  if(!owner(s)||s->ops.stock_cancel_iap2(s->ops.ctx,gen)!=1){
   s->state=CP_WCP_RESTORE_PENDING;return CP_WCP_WAIT;
  }
  s->requested=0;
  if(s->attempts>=CP_WCP_MAX_ATTEMPTS){
   s->state=CP_WCP_BLOCKED;return CP_WCP_DENIED;
  }
  if(!deadline(now,CP_WCP_RETRY_MS,&s->deadline_ms)){
   /* Native cancel has completed. Never leave OPENING with requested=0:
    * the next dispatch must not cancel the same operation again. */
   s->state=CP_WCP_BLOCKED;return CP_WCP_CLOCK;
  }
  s->state=CP_WCP_BACKOFF;return CP_WCP_WAIT;
 }
 if(s->state!=CP_WCP_DELAY&&s->state!=CP_WCP_BACKOFF)return CP_WCP_INVALID;
 if(s->attempts>=CP_WCP_MAX_ATTEMPTS){
  s->state=CP_WCP_BLOCKED;return CP_WCP_DENIED;
 }
 peer=s->ops.phone_iap2_sdp(s->ops.ctx,gen);
 if(peer==0){
  /* Not an open failure: back off rather than probing again immediately. */
  if(!deadline(now,CP_WCP_NO_SERVICE_MS,&s->deadline_ms))return CP_WCP_CLOCK;
  s->state=CP_WCP_BACKOFF;return CP_WCP_WAIT;
 }
 if(peer!=1){
  if(!deadline(now,CP_WCP_RETRY_MS,&s->deadline_ms))return CP_WCP_CLOCK;
  s->state=CP_WCP_BACKOFF;return CP_WCP_WAIT;
 }
 rc=s->ops.stock_open_iap2(s->ops.ctx,gen);
 if(rc==CP_WCP_OPEN_UNCERTAIN){
  s->attempts++;s->requested=1;s->state=CP_WCP_RESTORE_PENDING;
  return CP_WCP_WAIT;
 }
 if(rc==CP_WCP_OPEN_PENDING){
  s->attempts++;s->requested=1;
  if(!deadline(now,CP_WCP_OPEN_TIMEOUT_MS,&s->deadline_ms)){
   s->state=CP_WCP_RESTORE_PENDING;return CP_WCP_CLOCK;
  }
  s->state=CP_WCP_OPENING;
  return CP_WCP_WAIT;
 }
 if(rc==CP_WCP_OPEN_BUSY){
  if(!deadline(now,CP_WCP_RETRY_MS,&s->deadline_ms))return CP_WCP_CLOCK;
  s->state=CP_WCP_BACKOFF;return CP_WCP_WAIT;
 }
 s->state=CP_WCP_BLOCKED;return CP_WCP_DENIED;
}
