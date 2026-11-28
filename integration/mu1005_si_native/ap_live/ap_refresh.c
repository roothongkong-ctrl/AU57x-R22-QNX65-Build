#include "ap_refresh.h"
#include <string.h>
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
 uint32_t epoch=p->epoch,generation=p->bt_generation;
 enum cp_radio_band band=p->band;
 const struct cp_step04_session*bootstrap=p->bootstrap;
 if(mu_ap_live_read(&s->ap,q,ctx)!=0||
    cp_radio_channel_band(s->ap.channel)!=band){
  mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_PROOF;
 }
 /* A new attempt must never inherit credentials collected for its predecessor. */
 if(!current(p)||p->bootstrap!=bootstrap||p->epoch!=epoch||
    p->bt_generation!=generation||p->band!=band){
  mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_STALE;
 }
 s->epoch=epoch;s->generation=generation;s->band=band;s->prepared=1;
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

/* Revalidate session and original radio ownership before each driver write.
 * All calls are serialized by the original owner; no concurrent worker access. */
static int first_link_live(const struct cp_cloud_pipeline*p){
 return current(p)&&p->owner.wired_idle&&p->owner.bt_alive&&
  p->owner.wired_idle(p->owner.ctx)==1&&
  p->owner.bt_alive(p->owner.ctx,p->epoch,p->bt_generation)==1&&
  cp_s4_authenticated(p->bootstrap);
}
struct first_link_guard{
 const struct cp_cloud_pipeline*p;
 const struct mu_ap_refresh_slot*s;
 const struct mu_ap_ie_ops*ops;
};
static int guard_live(struct first_link_guard*g){
 return first_link_live(g->p)&&g->p->epoch==g->s->epoch&&
  g->p->bt_generation==g->s->generation&&g->p->band==g->s->band;
}
static int guard_query(void*v,unsigned n,char*out,size_t cap){
 struct first_link_guard*g=v;
 if(!guard_live(g))return -1;
 int rc=g->ops->query(g->ops->ctx,n,out,cap);
 return rc==0&&guard_live(g)?0:-1;
}
static int guard_exclusive(void*v,const uint8_t b[6],unsigned slot){
 struct first_link_guard*g=v;
 return guard_live(g)&&g->ops->exclusive(g->ops->ctx,b,slot)==1;
}
static int guard_write(void*v,unsigned slot,const uint8_t bt[6],const uint8_t id[6],int en){
 struct first_link_guard*g=v;
 if(!guard_exclusive(v,g->s->ap.bssid,slot)||!en)return -1;
 return g->ops->write(g->ops->ctx,slot,bt,id,en);
}
/* Do not mutate a foreign SSID or advertise a different Bluetooth identity.
 * Retain any uncertain lease for explicit recovery by the original AP owner. */
int mu_ap_refresh_prepare_first_link(struct mu_ap_refresh_slot*s,
 const struct cp_cloud_pipeline*p,struct mu_ap_ie_lease*lease,
 const struct mu_ap_ie_ops*ops,const uint8_t bt[6],const uint8_t id[6]){
 int r;
 if(!s||!p||!lease||lease->state||!ops||!ops->query||!ops->exclusive||!ops->write||!bt||!id)return CP_CLOUD_BAD;
 if(!first_link_live(p))return CP_CLOUD_PROOF;
 r=mu_ap_refresh_prepare(s,p,ops->query,ops->ctx);
 if(r!=CP_CLOUD_OK)return r;
 const struct cp_identification*i=p->bootstrap->identity;
 struct first_link_guard g={p,s,ops};
 const struct mu_ap_ie_ops guarded={&g,guard_query,guard_write,guard_exclusive};
 if(!guard_live(&g)||!i||!i->wireless_ssid||!i->bluetooth_mac6||
    strcmp(i->wireless_ssid,s->ap.ssid)||memcmp(i->bluetooth_mac6,bt,6)||
    s->band!=CP_RADIO_24||mu_ap_ie_install_first_free(lease,&guarded,&s->ap,bt,id)<0||
    !guard_live(&g)){
  mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_PROOF;
 }
 /* The original AP may change between its first snapshot and IE readback.
  * Re-read under the same owner proof, including SSID/key/band/BSSID.
  * Keep any acquired IE lease for explicit restoration on failure. */
 struct mu_ap_live after={0};
 int same=mu_ap_live_read(&after,guard_query,&g)==0&&
  !memcmp(&after,&s->ap,sizeof after)&&guard_live(&g);
 mu_ap_live_clear(&after,sizeof after);
 if(!same){mu_ap_live_clear(s,sizeof *s);return CP_CLOUD_PROOF;}
 return CP_CLOUD_OK;
}
