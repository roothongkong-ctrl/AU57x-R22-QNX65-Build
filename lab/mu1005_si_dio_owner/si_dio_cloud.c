/* SPDX-License-Identifier: GPL-3.0-only */
#include "si_dio_cloud.h"
static int configured(const MuSiDioCloud*d){
 return d&&d->original.admit&&d->original.cleanup&&d->env&&d->library&&d->endpoint_owned;
}
int mu_si_dio_after_wifi(MuSiDioCloud*d,struct cp_cloud_pipeline*p){
 if(!configured(d)||!p||!p->started||p->stopped||!p->epoch||!p->bootstrap||
    p->remote_phase||p->airplay||p->native_dio||p->rx_service)return CP_CLOUD_ORDER;
 if(p->handoff.phase!=CP_HO_WIFI_ASSOCIATED||p->radio.phase!=CP_RADIO_ASSOCIATED)
  return CP_CLOUD_RETRY;
 if(p->radio.epoch!=p->epoch||p->radio.selected!=p->band||
    !p->owner.wired_idle||p->owner.wired_idle(p->owner.ctx)!=1||
    !p->owner.bt_alive||p->owner.bt_alive(p->owner.ctx,p->epoch,p->bt_generation)!=1||
    !p->owner.ap_association||p->owner.ap_association(p->owner.ctx,p->epoch,p->band)!=1||
    !p->owner.si_single_owner||p->owner.si_single_owner(p->owner.ctx,p->epoch)!=1||
    !cp_s4_authenticated(p->bootstrap))return CP_CLOUD_PROOF;
 if(d->child.launch.attempted){
  if(d->child.launch.epoch!=p->epoch||!d->launched||d->child.launch.pid<=0)
   return CP_CLOUD_PROOF;
  /* Only the supervisor that spawned the child may reap it. Do not use
   * a recycled PID as proof or create another process on early exit. */
  return mu_dio_launch_reap(&d->child.launch)==1?CP_CLOUD_RETRY:CP_CLOUD_PROOF;
 }
 int rc=mu_si_dio_begin(&d->child,&d->original,p->epoch,NULL,d->env,d->library);
 if(rc!=0)return CP_CLOUD_PROOF;
 d->launched=1;
 return CP_CLOUD_RETRY;
}
int mu_si_dio_cloud_tick(MuSiDioCloud*d,struct cp_wcp_r45_remote*r,struct cp_s4_socket_io*w,
 struct cp_s4_tcp_bridge*t,const struct cp_wcp_r45_remote_owner*o,struct cp_wcp_autoconnect*a,
 struct cp_cloud_pipeline*p,const struct cp_cloud_owner*ops,const MuCprxRemoteEndpoint*e,
 const char*iface,uint64_t now){
 if(!configured(d)||!p||!ops||!ops->wired_idle)return CP_CLOUD_BAD;
 if(d->child.launch.attempted&&d->child.launch.epoch!=p->epoch)return CP_CLOUD_STALE;
 /* On USB takeover, pump the existing restore transaction, not START.
  * Do not report completed retirement while our actual child remains. */
 if(p->stopped||ops->wired_idle(ops->ctx)!=1){
  int rc=cp_wcp_r45_cloud_tick(r,w,t,o,a,p,ops,NULL,NULL,now);
  if(d->child.launch.attempted){
   if(!p->stopped||(p->remote_phase&&p->remote_phase!=4))return CP_CLOUD_RETRY;
   if(mu_si_dio_retire(&d->child,&d->original,p->epoch)!=0)return CP_CLOUD_RETRY;
   d->launched=0;
  }
  if(p->started&&p->stopped&&(!p->remote_phase||p->remote_phase==4)&&
     !p->remote_teardown_confirmed){
   int done=cp_cloud_end(p);
   return done==CP_CLOUD_OK?CP_CLOUD_STOPPED:CP_CLOUD_RETRY;
  }
  return rc;
 }
 if(!p->remote_phase){
  if(e||iface){
   if(!e||!iface||!d->launched||d->child.launch.pid<=0||
      e->epoch!=p->epoch||e->generation!=p->epoch||
      d->original.admit(d->original.ctx,p->epoch,d->library)!=1||
      d->endpoint_owned(d->original.ctx,d->child.launch.pid,p->epoch,e)!=1)
    return CP_CLOUD_PROOF;
   /* Recheck exact live child immediately before passing the endpoint. */
   if(mu_si_dio_after_wifi(d,p)!=CP_CLOUD_RETRY)return CP_CLOUD_PROOF;
  }
 }
 int rc=cp_wcp_r45_cloud_tick(r,w,t,o,a,p,ops,e,iface,now);
 if(rc!=CP_CLOUD_RETRY||p->stopped||p->remote_phase||
    p->handoff.phase!=CP_HO_WIFI_ASSOCIATED)return rc;
 return mu_si_dio_after_wifi(d,p);
}
