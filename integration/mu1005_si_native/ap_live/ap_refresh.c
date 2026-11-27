#include "ap_refresh.h"
static int referenced(const struct mu_ap_refresh_slot *s,const struct cp_cloud_pipeline *p){
 return p&&p->bootstrap&&p->bootstrap->wireless.session==&s->session;
}
int mu_ap_refresh_clear(struct mu_ap_refresh_slot*s,const struct cp_cloud_pipeline*p){
 if(!s||!p||referenced(s,p))return CP_CLOUD_BAD;
 mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_OK;
}
static int current(const struct cp_cloud_pipeline*p){
 return p&&p->started&&!p->stopped&&!p->radio_failure_pending&&p->bootstrap&&
  p->bootstrap->wireless_bound&&p->bootstrap->wireless.session&&
  p->epoch&&p->bt_generation&&p->radio.phase==CP_RADIO_CONNECTING&&
  p->radio.epoch==p->epoch&&p->band!=CP_RADIO_NONE&&p->radio.selected==p->band;
}
int mu_ap_refresh_prepare(struct mu_ap_refresh_slot*s,const struct cp_cloud_pipeline*p,mu_ap_query q,void*ctx){
 if(!s||!p||!q||referenced(s,p))return CP_CLOUD_BAD;
 mu_ap_live_clear(s,sizeof *s);
 if(!current(p))return CP_CLOUD_ORDER;
 if(mu_ap_live_read(&s->ap,q,ctx)!=0||
    cp_radio_channel_band(s->ap.channel)!=p->band){
  mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_PROOF;
 }
 s->epoch=p->epoch;s->generation=p->bt_generation;s->band=p->band;s->prepared=1;
 return CP_CLOUD_OK;
}
struct verify_ctx {struct mu_ap_refresh_slot*s;mu_ap_verify_current fn;void*ctx;};
static int verify(void*v,uint32_t epoch,enum cp_radio_band band,const struct cp_session*cfg){
 struct verify_ctx*c=v;struct mu_ap_refresh_slot*s=c->s;
 return cfg==&s->session&&epoch==s->epoch&&band==s->band&&
  c->fn(c->ctx,epoch,s->generation,band,&s->ap)==1;
}
int mu_ap_refresh_commit(struct mu_ap_refresh_slot*s,struct cp_cloud_pipeline*p,mu_ap_verify_current fn,void*ctx){
 if(!s||!p||!fn||referenced(s,p)||!s->prepared)return CP_CLOUD_BAD;
 if(!current(p))return CP_CLOUD_ORDER;
 if(s->epoch!=p->epoch||s->generation!=p->bt_generation||s->band!=p->band)
  return CP_CLOUD_STALE;
 /* Only Wi-Fi is replaced. Preserve original receiver identity and IP
  * storage; caller keeps these borrowed fields valid for the session. */
 if(mu_ap_live_session(&s->session,&s->ap,p->bootstrap->wireless.session)!=0)
  return CP_CLOUD_PROOF;
 struct verify_ctx v={s,fn,ctx};
 return cp_cloud_refresh_ap_config(p,&s->session,verify,&v);
}

/* Worker-only: use the existing stock AP snapshot and Device IE adapter
 * before publishing a live configuration to the authenticated bootstrap.
 * The caller owns the serialized OEM radio lease. A failed/uncertain write
 * retains lease state for explicit recovery; never clear foreign slots. */
int mu_ap_refresh_prepare_first_link(struct mu_ap_refresh_slot*s,
 const struct cp_cloud_pipeline*p,struct mu_ap_ie_lease*lease,
 const struct mu_ap_ie_ops*ops,const uint8_t bt[6],const uint8_t id[6]){
 int r;
 if(!s||!p||!lease||lease->state||!ops||!ops->query||!ops->exclusive)return CP_CLOUD_BAD;
 r=mu_ap_refresh_prepare(s,p,ops->query,ops->ctx);
 if(r!=CP_CLOUD_OK)return r;
 if(s->band!=CP_RADIO_24||mu_ap_ie_install_first_free(lease,ops,&s->ap,bt,id)<0){
  mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_PROOF;
 }
 return CP_CLOUD_OK;
}
