/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_wcp_r45_cloud.h"
int cp_wcp_r45_cloud_tick(struct cp_wcp_r45_remote*r,struct cp_s4_socket_io*w,
 struct cp_s4_tcp_bridge*t,const struct cp_wcp_r45_remote_owner*o,
 struct cp_wcp_autoconnect*a,struct cp_cloud_pipeline*p,const struct cp_cloud_owner*ops,
 const MuCprxRemoteEndpoint*e,const char*iface,uint64_t now){
 if(!r||!w||!t||!o||!a||!p||!ops||!ops->wired_idle||!ops->bt_alive||
    !t->session||p->bootstrap!=t->session||t->epoch!=t->session->epoch)
  return CP_CLOUD_BAD;
 if(p->started&&(p->epoch!=t->epoch||p->bt_generation!=t->generation||
    p->owner.ctx!=ops->ctx||p->owner.wired_idle!=ops->wired_idle||
    p->owner.bt_alive!=ops->bt_alive||!r->bound||r->worker!=w||r->bridge!=t||
    r->generation!=p->bt_generation||a->ops.ctx!=r))return CP_CLOUD_STALE;
 /* Once restoration begins, never pump another wireless bootstrap frame. */
 if(p->remote_wired_pending||p->remote_phase==4){
  if(w->attached)cp_s4_socket_abort(w);
  return cp_cloud_drive_remote_dio(p,now);
 }
 if(p->stopped)return CP_CLOUD_STOPPED;
 int idle=ops->wired_idle(ops->ctx);
 if(idle!=1){
  if(w->attached)cp_s4_socket_abort(w);
  if(p->started&&idle==0)return cp_cloud_wired_preempt(p);
  return CP_CLOUD_PROOF;
 }
 if(!p->started){
  if(!w->attached||w->bridge!=t)return CP_CLOUD_PROOF;
  if(cp_cloud_prefer_stock_dual_band(p)!=CP_CLOUD_OK)return CP_CLOUD_ORDER;
  int rc=cp_wcp_r45_bootstrap_tick(r,w,t,o,a,now);
  if(rc==CP_WCP_WAIT)return CP_CLOUD_RETRY;
  if(rc!=CP_WCP_OK)return CP_CLOUD_PROOF;
  /* Actual negotiated generation and fresh original MFi required. */
  rc=cp_wcp_begin_cloud(a,t->session,p,ops);
  return rc==CP_WCP_OK||rc==CP_WCP_WAIT?CP_CLOUD_RETRY:CP_CLOUD_PROOF;
 }
 if(p->handoff.bt_connected){
  int alive=ops->bt_alive(ops->ctx,p->epoch,p->bt_generation);
  if(alive==0){
   if(w->attached)cp_s4_socket_abort(w);
   int rc=cp_cloud_bt_closed(p,p->bt_generation);
   if(rc!=CP_CLOUD_OK)return rc;
  }else{
   if(alive!=1||!w->attached||w->bridge!=t)return CP_CLOUD_PROOF;
   int rc=cp_wcp_r45_bootstrap_tick(r,w,t,o,a,now);
   if(rc!=CP_WCP_OK)return CP_CLOUD_PROOF;
  }
 }
 return cp_cloud_drive_first_connect(p,e,iface,now);
}
