/* SPDX-License-Identifier: GPL-3.0-only
 * Deliberately use exactly one generation and OEM authority for the link,
 * original AA00-AA05 MFi transcript, and peer ACK; no self-signed shortcut.
 */
#include "cp_step04_session.h"
#include <string.h>
static int link_live(void *opaque,uint32_t epoch,uint32_t gen){
 struct cp_step04_session *s=opaque;
 return s&&s->epoch==epoch&&s->link.generation==gen&&
  s->ext.bt_owner_live(s->ext.opaque,epoch,gen)==1&&
  s->ext.wired_idle(s->ext.opaque)==1;
}
static int control_live(void *opaque,uint32_t epoch,uint32_t gen){
 struct cp_step04_session *s=opaque;
 return s&&s->epoch==epoch&&s->link.generation==gen&&
  s->link.phase==CP_LK_NORMAL&&
  s->ext.control_owner_live(s->ext.opaque,epoch,gen)==1&&
  s->ext.wired_idle(s->ext.opaque)==1;
}
static int wired_idle(void *opaque){
 struct cp_step04_session *s=opaque;
 return s&&s->ext.wired_idle(s->ext.opaque)==1;
}
static int peer_authenticated(void *opaque,uint32_t epoch,uint32_t gen){
 struct cp_step04_session *s=opaque;
 return s&&control_live(s,epoch,gen)&&
  s->ext.peer_auth_succeeded(s->ext.opaque,epoch,gen)==1;
}
static int enqueue_csm(void *opaque,uint32_t epoch,uint32_t gen,
                       const uint8_t *csm,size_t len,uint32_t token){
 struct cp_step04_session *s=opaque;
 if(!s||!control_live(s,epoch,gen))return -1;
 return cp_lk_queue_control(&s->link,csm,len,token,s->now)==CP_LK_OK?0:-1;
}
static int committed(void *opaque,uint32_t epoch,uint32_t gen,
                     uint32_t token,uint16_t id){
 struct cp_step04_session *s=opaque;
 (void)id;
 return s&&control_live(s,epoch,gen)&&s->ack_in_progress&&
  token&&s->acked_token==token;
}
static int link_control(void *opaque,const uint8_t *frame,size_t n){
 struct cp_step04_session *s=opaque;
 if(!s||s->link.phase!=CP_LK_NORMAL)return -1;
 if(!s->auth_ready){
  if(cp_auth_init(&s->auth,s->stock,&s->auth_owner,
                  s->epoch,s->link.generation)!=CP_AUTH_OK)return -1;
  s->auth_ready=1;
 }
 int r=cp_auth_receive(&s->auth,frame,n);
 if(r==CP_AUTH_UNRELATED){
  /* Unrelated iAP2 CONTROL messages are not swallowed or falsely approved. */
  return s->ext.handoff_other_csm(s->ext.opaque,frame,n)==1?0:-1;
 }
 return (r==CP_AUTH_TX_PENDING||r==CP_AUTH_OK)?0:-1;
}
static int link_acked(void *opaque,uint32_t token){
 struct cp_step04_session *s=opaque;
 if(!s||!s->auth_ready||s->ack_in_progress)return -1;
 s->ack_in_progress=1;s->acked_token=token;
 int r=cp_auth_tx_confirm(&s->auth,token);
 s->ack_in_progress=0;s->acked_token=0;
 return r==CP_AUTH_OK?0:-1;
}
int cp_s4_init(struct cp_step04_session *s,struct cp_stock_mfi *stock,
               uint32_t epoch,const struct cp_s4_external *ext){
 if(!s||!stock||!ext||!epoch||!ext->bt_owner_live||
  !ext->control_owner_live||!ext->wired_idle||
  !ext->peer_auth_succeeded||!ext->handoff_other_csm)return CP_LK_BAD;
 memset(s,0,sizeof *s);s->stock=stock;s->ext=*ext;s->epoch=epoch;
 s->link_owner.opaque=s;s->link_owner.live=link_live;
 s->link_owner.control=link_control;s->link_owner.acked=link_acked;
 s->auth_owner.opaque=s;s->auth_owner.control_live=control_live;
 s->auth_owner.wired_idle=wired_idle;s->auth_owner.send_csm=enqueue_csm;
 s->auth_owner.tx_committed=committed;
 s->auth_owner.peer_auth_succeeded=peer_authenticated;
 cp_lk_init(&s->link);return CP_LK_OK;
}
int cp_s4_accept_rf(struct cp_step04_session *s,const AuRfWire *w,uint64_t now){
 if(!s||!w)return CP_LK_BAD;
 s->now=now;
 int r=cp_lk_accept_rf(&s->link,w,s->epoch,&s->link_owner,now);
 if(r!=CP_LK_OK&&r!=CP_LK_BACKPRESSURE&&r!=CP_LK_STALE){
  cp_s4_revoke(s);
 }
 if(s->link.phase==CP_LK_DEAD)cp_s4_revoke(s);
 return r;
}
int cp_s4_flush_rf(struct cp_step04_session *s,AuRfIngress *rf){
 if(!s)return CP_LK_BAD;
 return cp_lk_drain_rf(&s->link,rf);
}
int cp_s4_tick(struct cp_step04_session *s,uint64_t now){
 if(!s)return CP_LK_BAD;
 s->now=now;
 int r=cp_lk_tick(&s->link,now);
 if(s->link.phase==CP_LK_DEAD)cp_s4_revoke(s);
 return r;
}
int cp_s4_authenticated(struct cp_step04_session *s){
 if(!s||!s->auth_ready||s->link.phase!=CP_LK_NORMAL)return 0;
 return cp_auth_live(&s->auth);
}
void cp_s4_revoke(struct cp_step04_session *s){
 if(!s)return;
 cp_lk_revoke(&s->link);
 if(s->auth_ready)cp_auth_revoke(&s->auth);
 s->ack_in_progress=0;s->acked_token=0;
}
