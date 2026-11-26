/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_wcp_r45_remote.h"
#include <string.h>

static int valid_mac(const uint8_t p[6]){
 unsigned any=0,all=255;
 for(unsigned i=0;i<6;i++){any|=p[i];all&=p[i];}
 return any!=0 && all!=255;
}
/* Every use of a transport assertion rechecks the REAL socket ownership,
 * R44 parser generation, session epoch, SDP publication and USB priority.
 * The R14 wire MAC is a claimed value until an independent OEM paired-ACL
 * provider authenticates it: never let CONNECT alone authorize CarPlay. */
static int live(const struct cp_wcp_r45_remote *r,uint32_t generation){
 const struct cp_s4_socket_io *w;
 const struct cp_s4_tcp_bridge *t;
 const struct cp_step04_session *s;
 if(!r||!r->bound||!generation||r->generation!=generation||
    !r->worker||!r->bridge)return 0;
 w=r->worker;t=r->bridge;s=t->session;
 if(!w->attached||w->fd<0||w->bridge!=t||!w->peer_verified||
    !s||t->closed||!t->connected||!t->epoch||
    t->epoch!=r->epoch||s->epoch!=r->epoch||
    t->generation!=r->generation||
    s->link.generation!=r->generation||s->link.phase==CP_LK_DEAD||
    s->link.phase==CP_LK_IDLE||s->link.epoch!=r->epoch||
    !valid_mac(s->link.peer_mac)||
    memcmp(s->link.peer_mac,r->peer,6)!=0||
    !r->o.owner_verified||!r->o.server_published||
    !r->o.paired_acl_authorized||!r->o.wired_idle||
    !r->o.transport_verified||
    /* Existing R44 socket authorizer must continue to approve this fd. */
    w->peer_verified(w->opaque,w->fd,r->epoch)!=1||
    !t->peer_authorized||t->peer_authorized(t->opaque,r->epoch)!=1||
    r->o.owner_verified(r->o.ctx,r->epoch,generation)!=1||
    r->o.server_published(r->o.ctx,r->epoch,generation)!=1||
    r->o.paired_acl_authorized(r->o.ctx,r->epoch,generation,r->peer)!=1||
    r->o.wired_idle(r->o.ctx)!=1||
    r->o.transport_verified(r->o.ctx,w->fd,r->epoch,generation)!=1)
  return 0;
 return 1;
}
int cp_wcp_r45_remote_bind(struct cp_wcp_r45_remote *r,
 struct cp_s4_socket_io *w,struct cp_s4_tcp_bridge *t,
 const struct cp_wcp_r45_remote_owner *o){
 const struct cp_step04_session *s;
 if(!r||r->bound||!w||!t||!o||!o->owner_verified||
    !o->server_published||!o->paired_acl_authorized||
    !o->wired_idle||!o->transport_verified||
    !w->attached||w->bridge!=t||t->closed||!t->connected||
    !t->generation||!t->epoch)return -1;
 s=t->session;
 if(!s||s->epoch!=t->epoch||
    s->link.generation!=t->generation||s->link.epoch!=t->epoch||
    !valid_mac(s->link.peer_mac))return -1;
 memset(r,0,sizeof *r);
 r->worker=w;r->bridge=t;r->o=*o;
 r->epoch=t->epoch;r->generation=t->generation;
 memcpy(r->peer,s->link.peer_mac,6);
 r->bound=1;
 if(!live(r,r->generation)){
  memset(r,0,sizeof *r);return -1;
 }
 return 0;
}
static int owner(void *p,uint32_t g){return live((struct cp_wcp_r45_remote*)p,g);}
static int paired(void *p,uint32_t g){return live((struct cp_wcp_r45_remote*)p,g);}
static int wired(void *p){
 struct cp_wcp_r45_remote *r=p;
 return r&&r->bound&&live(r,r->generation);
}
/* MU1005 BlueSDK event 2 only proves original RFCOMM transport, not an
 * established iAP2 CONTROL link.  A real phone must exchange the marker,
 * valid peer LSP/SYN and ACK completing R44's link state machine first.
 * R44 separately runs the original AA00-AA05 MFi exchange; this function
 * MUST NOT infer MFi from RFCOMM or from the iAP2 marker alone. */
static int connected(void *p,uint32_t g){
 const struct cp_wcp_r45_remote *r=p;
 const struct cp_step04_link *l;
 if(!live(r,g))return 0;
 l=&r->bridge->session->link;
 return l->phase==CP_LK_NORMAL && l->have_peer_syn==1;
}
static int sdp(void *p,uint32_t g){(void)p;(void)g;return -1;}
static int open_client(void *p,uint32_t g){(void)p;(void)g;return -1;}
static int cancel_client(void *p,uint32_t g){(void)p;(void)g;return 0;}
int cp_wcp_r45_remote_ops(struct cp_wcp_r45_remote *r,struct cp_wcp_bt_ops *out){
 if(!r||!out||!live(r,r->generation))return -1;
 memset(out,0,sizeof *out);
 out->ctx=r;
 out->stock_owner_verified=owner;
 out->paired_acl_verified=paired;
 out->wired_idle_verified=wired;
 out->stock_iap2_connected=connected;
 /* No guessed client SDP/open/cancel ABI; original server path only. */
 out->phone_iap2_sdp=sdp;
 out->stock_open_iap2=open_client;
 out->stock_cancel_iap2=cancel_client;
 return 0;
}
int cp_wcp_r45_bootstrap_tick(struct cp_wcp_r45_remote *r,
 struct cp_s4_socket_io *w,struct cp_s4_tcp_bridge *t,
 const struct cp_wcp_r45_remote_owner *o,struct cp_wcp_autoconnect *ac,
 uint64_t now){
 struct cp_wcp_bt_ops ops;
 int rc;
 if(!r||!w||!t||!o||!ac||!w->attached||w->bridge!=t||!t->session)
  return CP_WCP_INVALID;
 if(r->bound&&(r->worker!=w||r->bridge!=t||ac->ops.ctx!=r||
              ac->generation!=r->generation))return CP_WCP_DENIED;
 if(!r->bound&&(ac->state!=CP_WCP_IDLE||ac->generation||ac->requested))
  return CP_WCP_DENIED;
 rc=cp_s4_socket_poll(w,now);
 if(rc!=CP_LK_OK)return CP_WCP_DENIED;
 if(!t->connected)return CP_WCP_WAIT;
 if(!r->bound){
  if(cp_wcp_r45_remote_bind(r,w,t,o)!=0)return CP_WCP_DENIED;
  if(cp_wcp_r45_remote_ops(r,&ops)!=0||
     cp_wcp_bt_init(ac,&ops)!=CP_WCP_OK||
     cp_wcp_bt_acl_up(ac,r->generation,now)!=CP_WCP_OK)
   return CP_WCP_DENIED;
 }
 rc=cp_wcp_bt_tick(ac,r->generation,now);
 if(rc<0)return rc;
 if(rc!=CP_WCP_LINK_READY)return CP_WCP_WAIT;
 return cp_s4_authenticated(t->session)?CP_WCP_OK:CP_WCP_WAIT;
}
void cp_wcp_r45_remote_unbind(struct cp_wcp_r45_remote *r){
 if(r)memset(r,0,sizeof *r);
}
