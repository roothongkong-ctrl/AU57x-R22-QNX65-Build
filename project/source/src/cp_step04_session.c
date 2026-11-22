/* SPDX-License-Identifier: GPL-3.0-only
 * Deliberately use exactly one generation and OEM authority for the link,
 * original AA00-AA05 MFi transcript, and peer ACK; no self-signed shortcut.
 */
#include "cp_step04_session.h"
#include "cp_event.h"
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
static int respond_identification(struct cp_step04_session *s){
 uint8_t frame[CP_LK_FRAME_MAX];
 size_t n=0;
 if(!s||!s->identity)return -1;
 if(s->identity_accepted||s->identity_pending)return 0;
 if(cp_encode_identification_1d01(s->identity,frame,sizeof frame,&n)!=CP_OK||
    n>CP_LK_FRAME_MAX-10u)return -1;
 if(cp_lk_queue_control(&s->link,frame,n,0x40000001u,s->now)!=CP_LK_OK)return -1;
 s->identity_token=0x40000001u;
 s->identity_pending=1;s->identity_sent=1;
 return 0;
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
  if(s->identity&&n>=6&&frame[0]==0x40&&frame[1]==0x40){
   unsigned id=(unsigned)frame[4]*256u+frame[5];
   if(id==0x1d00)return n==6?respond_identification(s):-1;
   if(id==0x1d02){
    if(n!=6||!s->identity_sent||s->identity_pending)return -1;
    s->identity_accepted=1;return 0;
   }
   if(id==0x1d03)return -1; /* Phone rejected this identity. */
  }
  /* Route Wi-Fi / availability directly over the original iAP2 CONTROL
   * session; unrelated identification and media CSMs stay with OEM owner. */
  if(s->wireless_bound){
   struct cp_event event;
   struct cp_oem_assertions proof={0};
   int wr;
   if(cp_decode_event(frame,n,&event)!=0)return -1;
   if(event.type!=CP_EVENT_OTHER){
    if(s->identity&&!s->identity_accepted)return 0;
    if(s->wireless_proof(s->wireless_ctx,s->epoch,
                         s->link.generation,&proof)!=1)return 0;
    wr=cp_step04_dispatch(&s->auth,&s->wireless,&proof,frame,n);
    if(wr==CP_WL_OK||wr==CP_WL_NOT_READY||
       wr==CP_WL_PHONE_UNAVAILABLE||wr==CP_WL_LIMIT_REACHED)return 0;
    /* Phone can send 4300 while 5703 is still awaiting link ACK.
     * Keep its offer and retry only AFTER the earlier frame is committed. */
    if(wr==CP_WL_TRANSPORT_ERROR&&s->wireless_pending)return 0;
    return -1;
   }
  }
  return s->ext.handoff_other_csm(s->ext.opaque,frame,n)==1?0:-1;
 }
 return (r==CP_AUTH_TX_PENDING||r==CP_AUTH_OK)?0:-1;
}
static int link_acked(void *opaque,uint32_t token){
 struct cp_step04_session *s=opaque;
 if(!s||!s->auth_ready||s->ack_in_progress)return -1;
 if(s->identity_pending&&token==s->identity_token){
  s->identity_pending=0;s->identity_token=0;return 0;
 }
 /* Post-MFi Wi-Fi / CarPlay CONTROL frames use a distinct application
  * token space; their ACK must never be mistaken for an AA01/AA03 ACK. */
 if(s->wireless_pending&&token==s->wireless_token){
  if(!cp_s4_authenticated(s))return -1;
  s->wireless_pending=0;s->wireless_token=0;return 0;
 }
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
 else if(r==CP_LK_OK&&s->wireless_bound)
  (void)cp_s4_wireless_pump(s);
 return r;
}
int cp_s4_authenticated(struct cp_step04_session *s){
 if(!s||!s->auth_ready||s->link.phase!=CP_LK_NORMAL)return 0;
 return cp_auth_live(&s->auth);
}
int cp_s4_bind_identity(struct cp_step04_session *s,
                        const struct cp_identification *id){
 if(!s||!id||s->identity||!s->epoch||
    s->link.phase!=CP_LK_IDLE)return CP_LK_BAD;
 if(s->wireless_bound&&s->wireless.session&&
    (!s->wireless.session->wifi.ssid||
     strcmp(s->wireless.session->wifi.ssid,id->wireless_ssid)!=0))
    return CP_LK_BAD;
 s->identity=id;return CP_LK_OK;
}
int cp_s4_bind_wireless(struct cp_step04_session *s,const struct cp_session *config,
                        int (*prove)(void *,uint32_t,uint32_t,
                                     struct cp_oem_assertions *),void *ctx){
 if(!s||!config||!prove||s->wireless_bound||
    !s->epoch||s->link.phase!=CP_LK_IDLE)return CP_LK_BAD;
 cp_wireless_init(&s->wireless,config,cp_s4_wireless_send,s);
 s->wireless_proof=prove;s->wireless_ctx=ctx;s->wireless_bound=1;
 return CP_LK_OK;
}
/* Send one pending CarPlay start after a real OEM receiver becomes ready.
 * Called on the authorized owner tick, not from the Bluetooth callback. */
int cp_s4_wireless_pump(struct cp_step04_session *s){
 struct cp_oem_assertions p={0};
 if(!s||!s->wireless_bound||s->wireless_pending||
    s->wireless.start_sent||!s->wireless.availability_seen||
    (s->identity&&!s->identity_accepted)||
    !cp_s4_authenticated(s))return CP_WL_NOT_READY;
 if(s->wireless_proof(s->wireless_ctx,s->epoch,s->link.generation,&p)!=1||
    p.wired_active||!p.sole_dio_owner||
    !p.verified_control_session||!p.live_hotspot||
    !p.network_airplay_ready)return CP_WL_NOT_READY;
 s->wireless.authenticated=1;
 s->wireless.hotspot_ready=1;
 s->wireless.airplay_ready=1;
 return cp_wireless_start_if_ready(&s->wireless);
}
/* Callback for cp_wireless_init(): transmit 5703 / 4301 on the same
 * authenticated iAP2 CONTROL session after the stock MFi handshake. */
int cp_s4_wireless_send(void *ctx,const uint8_t *frame,size_t len){
 struct cp_step04_session *s=ctx;
 uint32_t token;
 if(!s||!frame||!len||!cp_s4_authenticated(s)||s->wireless_pending||
    s->wireless_seq>=0x7fffffffu)return -1;
 token=0x80000000u|(++s->wireless_seq);
 if(cp_lk_queue_control(&s->link,frame,len,token,s->now)!=CP_LK_OK)return -1;
 s->wireless_token=token;s->wireless_pending=1;
 return 0;
}
void cp_s4_revoke(struct cp_step04_session *s){
 if(!s)return;
 cp_lk_revoke(&s->link);
 if(s->auth_ready)cp_auth_revoke(&s->auth);
 s->ack_in_progress=0;s->acked_token=0;
 s->wireless_pending=0;s->wireless_token=0;
 s->wireless_bound=0;
 s->identity_pending=0;s->identity_token=0;s->identity_accepted=0;
}
