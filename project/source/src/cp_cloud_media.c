/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_cloud_media.h"
#include <string.h>
int cp_cloud_media_bind(struct cp_cloud_media_router *r,
                        struct cp_cloud_pipeline *session,const struct cp_cloud_media_ops *o){
 if(!r||!session||!o||!o->on_owner_thread||!o->source_verified||!o->native_deliver||
    !session->started||session->stopped||!session->epoch)return CP_FWD_INVALID;
 /* Stock AirPlay owns video/audio decoding; this raw byte router is only
  * a standalone test adapter and may NEVER inject into native DIO. */
 if(session->native_dio)return CP_FWD_OWNER;
 memset(r,0,sizeof *r);r->pipeline=session;r->ops=*o;r->epoch=session->epoch;
 return CP_FWD_OK;
}
void cp_cloud_media_close(struct cp_cloud_media_router *r){
 if(r){r->closed=1;memset(r->last_seq,0,sizeof r->last_seq);}
}
int cp_cloud_media_forward(struct cp_cloud_media_router *r,enum cp_cloud_media type,
                           const uint8_t *payload,size_t n,uint32_t seq,uint32_t epoch){
 struct cp_cloud_pipeline *p;
 if(!r||!payload||!seq||!n||type<CP_CLOUD_VIDEO||type>CP_CLOUD_STEERING)
  return CP_FWD_INVALID;
 if(r->closed||!r->pipeline)return CP_FWD_ORDER;
 p=r->pipeline;
 if(p->native_dio){cp_cloud_media_close(r);return CP_FWD_OWNER;}
 if(!epoch||epoch!=r->epoch||p->epoch!=epoch)return CP_FWD_STALE;
 if(n>CP_CLOUD_MEDIA_MAX||((type==CP_CLOUD_HMI||type==CP_CLOUD_STEERING)&&n>256u))
  return CP_FWD_INVALID;
 if(seq<=r->last_seq[type])return CP_FWD_ORDER;
 if(p->stopped||!p->started||p->handoff.wired_active||
    (p->handoff.phase!=CP_HO_INNER_IAP_AUTH&&p->handoff.phase!=CP_HO_MEDIA_ACTIVE)||
    !p->owner.wired_idle||p->owner.wired_idle(p->owner.ctx)!=1||
    !p->owner.si_single_owner||p->owner.si_single_owner(p->owner.ctx,epoch)!=1||
    !p->airplay||p->airplay->generation!=epoch||
    mu34_ap_server_state(p->airplay)!=MU34_AP_LISTENING_VERIFIED){
  cp_cloud_media_close(r);return CP_FWD_OWNER;
 }
 if(r->ops.on_owner_thread(r->ops.ctx,epoch)!=1||
    r->ops.source_verified(r->ops.ctx,epoch,type,seq)!=1){
  cp_cloud_media_close(r);return CP_FWD_OWNER;
 }
 int res=r->ops.native_deliver(r->ops.ctx,epoch,type,payload,n,seq);
 if(res==0)return CP_FWD_BACKPRESSURE;
 if(res!=1){cp_cloud_media_close(r);return CP_FWD_OWNER;}
 r->last_seq[type]=seq;
 return CP_FWD_OK;
}
