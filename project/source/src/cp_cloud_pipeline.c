/* SPDX-License-Identifier: GPL-3.0-only
 * State-machine integration only; does not assert the availability of
 * Wi-Fi 5GHz, the CarPlay Bonjour service, native DIO or an OEM MFi owner.
 */
#include "cp_cloud_pipeline.h"
#include <string.h>
static int actual_wireless_proof(void *opaque,uint32_t epoch,uint32_t generation,
                                 struct cp_oem_assertions *out){
 struct cp_cloud_pipeline *p=opaque;
 const char *iface;
 if(!out)return 0;
 memset(out,0,sizeof *out);
 if(!p||!p->started||p->stopped||!p->bootstrap||
    epoch!=p->epoch||generation!=p->bt_generation||
    p->bootstrap->epoch!=epoch||
    p->bootstrap->link.generation!=generation||
    !p->owner.wired_idle||p->owner.wired_idle(p->owner.ctx)!=1||
    !p->owner.bt_alive||
    p->owner.bt_alive(p->owner.ctx,epoch,generation)!=1)return 0;
 out->verified_mfi=cp_s4_authenticated(p->bootstrap);
 out->verified_control_session=out->verified_mfi;
 out->sole_dio_owner=p->owner.si_single_owner(p->owner.ctx,epoch)==1;
 out->wired_active=0;
 out->live_hotspot=(p->radio.epoch==epoch&&p->band!=CP_RADIO_NONE&&
     p->radio.selected==p->band&&
     (p->radio.phase==CP_RADIO_CONNECTING||
      p->radio.phase==CP_RADIO_ASSOCIATED)&&
     p->owner.radio_capability(p->owner.ctx,p->band)==1&&
     p->owner.ap_hotspot_verified(p->owner.ctx,epoch,p->band)==1);
 if(!out->live_hotspot||p->radio.phase!=CP_RADIO_ASSOCIATED||
    p->owner.ap_association(p->owner.ctx,epoch,p->band)!=1||
    !out->sole_dio_owner||!p->airplay||
    p->airplay->generation!=epoch||
    mu34_ap_server_poll(p->airplay,epoch)!=MU34_AP_R_LISTENING_VERIFIED)
  return 1;
 iface=mu34_ap_server_interface(p->airplay);
 out->network_airplay_ready=iface[0]&&
  p->airplay->expected_port==CP_CLOUD_PORT&&
  p->owner.bonjour_identity_verified(p->owner.ctx,epoch,iface)==1&&
  p->owner.airplay_listener_verified(p->owner.ctx,epoch,iface,CP_CLOUD_PORT)==1;
 return 1;
}
static int eligible(struct cp_cloud_pipeline *s,int require_bt){
 if(!s||!s->started||s->stopped||!s->epoch)return 0;
 if(!s->owner.wired_idle||s->owner.wired_idle(s->owner.ctx)!=1)return 0;
 if(require_bt&&(!s->owner.bt_alive||
     s->owner.bt_alive(s->owner.ctx,s->epoch,s->bt_generation)!=1))return 0;
 return 1;
}
static int stop(struct cp_cloud_pipeline *s){
 if(!s)return CP_CLOUD_BAD;
 if(s->started&&!s->stopped){
  s->stopped=1;
  if(s->radio.epoch)(void)cp_radio_revoke(&s->radio,s->epoch,0);
  if(s->bootstrap)cp_s4_revoke(s->bootstrap);
  (void)cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_FAILURE,1);
 }
 return CP_CLOUD_PROOF;
}
void cp_cloud_init(struct cp_cloud_pipeline *s){
 if(!s)return;
 memset(s,0,sizeof *s);cp_radio_init(&s->radio);cp_ho_init(&s->handoff);
}
int cp_cloud_begin(struct cp_cloud_pipeline *s,struct cp_step04_session *b,
                  const struct cp_cloud_owner *ops,uint32_t epoch,uint32_t gen){
 if(!s||!b||!ops||!epoch||!gen)return CP_CLOUD_BAD;
 if(s->started||s->stopped)return CP_CLOUD_ORDER;
 if(s->bootstrap&&s->bootstrap!=b)return CP_CLOUD_STALE;
 if(b->epoch!=epoch||b->link.generation!=gen)return CP_CLOUD_STALE;
 if(!ops->bt_alive||!ops->wired_idle||!ops->radio_capability||
    !ops->ap_prepare||!ops->ap_association||!ops->ap_disconnected||
    !ops->carplay_request_sent||!ops->si_single_owner||
    !ops->bonjour_identity_verified||!ops->airplay_listener_verified||
    !ops->inner_iap2_mfi_verified||!ops->media_verified||
    !ops->teardown_confirmed||!ops->ap_hotspot_verified||
    !ops->airplay_session_connected)return CP_CLOUD_BAD;
 if(ops->wired_idle(ops->ctx)!=1||ops->bt_alive(ops->ctx,epoch,gen)!=1)
  return CP_CLOUD_PROOF;
 s->owner=*ops;s->bootstrap=b;s->epoch=epoch;s->bt_generation=gen;
 if(cp_ho_begin(&s->handoff,epoch)!=CP_HO_OK)return CP_CLOUD_ORDER;
 if(cp_ho_signal(&s->handoff,epoch,CP_HO_EV_BT_CONNECTED,1)!=CP_HO_OK)
  return CP_CLOUD_ORDER;
 s->started=1;
 return CP_CLOUD_OK;
}
int cp_cloud_bind_bootstrap(struct cp_cloud_pipeline *p,
 struct cp_step04_session *b,const struct cp_session *config,
 const struct cp_identification *identity){
 uint8_t bytes[CP_LK_FRAME_MAX-10u];size_t n=0;
 if(!p||!b||!config||!identity||p->started||p->stopped||
    p->bootstrap||b->link.phase!=CP_LK_IDLE||!b->epoch||
    !config->wifi.ssid||!identity->wireless_ssid||
    strcmp(config->wifi.ssid,identity->wireless_ssid)!=0||
    config->airplay_port!=CP_CLOUD_PORT||
    cp_encode_identification_1d01(identity,bytes,sizeof bytes,&n)!=CP_OK||
    cp_encode_wifi_5703(&config->wifi,bytes,sizeof bytes,&n)!=CP_OK||
    cp_encode_start_4301(config,bytes,sizeof bytes,&n)!=CP_OK)
  return CP_CLOUD_BAD;
 if(cp_s4_bind_identity(b,identity)!=CP_LK_OK||
    cp_s4_bind_wireless(b,config,actual_wireless_proof,p)!=CP_LK_OK)
  return CP_CLOUD_BAD;
 p->bootstrap=b;
 return CP_CLOUD_OK;
}
int cp_cloud_bind_airplay(struct cp_cloud_pipeline *s,Mu34ApServer *ap){
 if(!s||!s->started||s->stopped||!ap)return CP_CLOUD_BAD;
 if(s->airplay||ap->generation!=s->epoch||ap->state==MU34_AP_FAILED)
  return CP_CLOUD_PROOF;
 s->airplay=ap;return CP_CLOUD_OK;
}
/* R69: use the original DIO singleton, typed native CF objects and owner
 * dispatch queue. This code does not load an OEM image or invent an owner. */
static int native_valid(struct cp_cloud_pipeline *p){
 MuDioNative *n;
 if(!p||!(n=p->native_dio)||!n->initialized||!n->server||
    !n->manager||!n->handle||!n->iface||!n->get_queue||!n->queue||
    !n->server_slot||*n->server_slot!=n->server||
    n->get_queue(n->server)!=n->queue||!n->guard.verified_library||
    n->guard.verified_library(n->guard.opaque,n->handle)!=1||
    !n->guard.owner_exclusive||
    n->guard.owner_exclusive(n->guard.opaque,n->server,n->manager)!=1||
    !n->ap.binding.cf_string_ok||
    n->ap.binding.cf_string_ok(n,n->iface)!=1||
    n->ap.binding.opaque!=n||n->ap.binding.server!=n->server||
    n->ap.binding.owner!=n->manager||n->ap.generation!=p->epoch||
    p->airplay!=&n->ap)return 0;
 return 1;
}
int cp_cloud_bind_native_dio(struct cp_cloud_pipeline *p,MuDioNative *n){
 if(!p||!n||!p->started||p->stopped||p->airplay||p->native_dio||
    !n->initialized||n->ap.generation!=p->epoch||
    n->ap.state!=MU34_AP_BOUND||!n->queue||!n->get_queue||
    !n->server_slot||!n->server||!n->manager||!n->iface||
    !n->guard.verified_library||!n->guard.owner_exclusive||
    !n->ap.binding.cf_string_ok||
    n->guard.verified_library(n->guard.opaque,n->handle)!=1||
    n->guard.owner_exclusive(n->guard.opaque,n->server,n->manager)!=1||
    *n->server_slot!=n->server||n->get_queue(n->server)!=n->queue||
    n->ap.binding.opaque!=n||n->ap.binding.server!=n->server||
    n->ap.binding.owner!=n->manager||
    n->ap.binding.cf_string_ok(n,n->iface)!=1)return CP_CLOUD_PROOF;
 p->native_dio=n;p->airplay=&n->ap;
 return CP_CLOUD_OK;
}
int cp_cloud_drive_native(struct cp_cloud_pipeline *p,uint64_t now){
 if(!p||!p->native_dio)return CP_CLOUD_BAD;
 if(!native_valid(p))return stop(p);
 return cp_cloud_drive_start(p,p->native_dio->iface,now);
}
/* Accept native stop only after the OEM confirms cleanup and original
 * interface restoration; preserve owner for retries otherwise. */
static int native_restore(struct cp_cloud_pipeline *p){
 int r;
 if(!p||!p->native_dio)return CP_CLOUD_OK;
 if(!p->native_dio->initialized){
  p->native_dio=0;p->airplay=0;return CP_CLOUD_OK;
 }
 r=mu_dio_restore(p->native_dio);
 if(r==1)return CP_CLOUD_RETRY;
 if(r!=0)return CP_CLOUD_PROOF;
 p->native_dio=0;p->airplay=0;
 return CP_CLOUD_OK;
}
int cp_cloud_confirm_bootstrap_auth(struct cp_cloud_pipeline *s){
 int cap24,cap5;
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_BT_LINK)return CP_CLOUD_ORDER;
 if(!s->bootstrap||s->bootstrap->epoch!=s->epoch||
    s->bootstrap->link.generation!=s->bt_generation||
    !cp_s4_authenticated(s->bootstrap))return CP_CLOUD_PROOF;
 cap24=s->owner.radio_capability(s->owner.ctx,CP_RADIO_24)==1;
 cap5=s->owner.radio_capability(s->owner.ctx,CP_RADIO_5)==1;
 if(cp_radio_begin(&s->radio,s->epoch,cap24,cap5,1,1)!=CP_RADIO_OK)
  return CP_CLOUD_PROOF;
 if(cp_ho_signal(&s->handoff,s->epoch,
    CP_HO_EV_BOOTSTRAP_MFI_AUTHENTICATED,1)!=CP_HO_OK)return stop(s);
 return CP_CLOUD_OK;
}
int cp_cloud_radio_start(struct cp_cloud_pipeline *s){
 enum cp_radio_band band=CP_RADIO_NONE;
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_BOOTSTRAP_AUTH)return CP_CLOUD_ORDER;
 if(cp_radio_select(&s->radio,s->epoch,1,&band)!=CP_RADIO_OK)
  return CP_CLOUD_ORDER;
 if(s->owner.ap_prepare(s->owner.ctx,s->epoch,band)!=1)return stop(s);
 if(cp_radio_connect_requested(&s->radio,s->epoch,1)!=CP_RADIO_OK)return stop(s);
 s->band=band;
 return CP_CLOUD_OK;
}
int cp_cloud_radio_result(struct cp_cloud_pipeline *s,int success,int disconnected){
 int r;
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_BOOTSTRAP_AUTH||
    s->radio.phase!=CP_RADIO_CONNECTING)return CP_CLOUD_ORDER;
 if(success!=0&&success!=1)return CP_CLOUD_BAD;
 if(success){
  if(disconnected!=0||s->owner.ap_association(s->owner.ctx,s->epoch,s->band)!=1)
    return CP_CLOUD_PROOF;
  r=cp_radio_association_result(&s->radio,s->epoch,1,1,s->band,0);
  if(r!=CP_RADIO_OK)return stop(s);
  if(cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_WIFI_AP_AND_PHONE_CONFIRMED,1)!=CP_HO_OK)
   return stop(s);
  return CP_CLOUD_OK;
 }
 if(!disconnected||s->owner.ap_disconnected(s->owner.ctx,s->epoch,s->band)!=1)
  return CP_CLOUD_RETRY; /* do not retry until actual disconnect */
 r=cp_radio_association_result(&s->radio,s->epoch,1,0,s->band,1);
 if(r==CP_RADIO_PENDING)return CP_CLOUD_RETRY;
 return stop(s);
}
int cp_cloud_prepare_receiver(struct cp_cloud_pipeline *s,const void *iface_cf){
 Mu34ApResult r;
 Mu34ApState state;
 const char *iface;
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_WIFI_ASSOCIATED)return CP_CLOUD_ORDER;
 if(!s->airplay||s->airplay->generation!=s->epoch||
    s->radio.epoch!=s->epoch||s->radio.phase!=CP_RADIO_ASSOCIATED||
    !s->owner.ap_association||
    s->owner.ap_association(s->owner.ctx,s->epoch,s->band)!=1||
    s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1)
  return CP_CLOUD_PROOF;
 state=mu34_ap_server_state(s->airplay);
 if(state==MU34_AP_BOUND){
  if(!iface_cf)return CP_CLOUD_BAD;
  r=mu34_ap_server_set_interface(s->airplay,s->epoch,iface_cf);
  if(r!=MU34_AP_OK)return CP_CLOUD_PROOF;
  state=MU34_AP_INTERFACE_SET;
 }
 if(state==MU34_AP_INTERFACE_SET){
  r=mu34_ap_server_start(s->airplay,s->epoch,CP_CLOUD_PORT);
 }else if(state==MU34_AP_START_REQUESTED||state==MU34_AP_LISTENING_VERIFIED){
  r=mu34_ap_server_poll(s->airplay,s->epoch);
 }else return CP_CLOUD_PROOF;
 if(r==MU34_AP_R_START_REQUESTED)return CP_CLOUD_RETRY;
 if(r!=MU34_AP_R_LISTENING_VERIFIED)return CP_CLOUD_PROOF;
 if(s->airplay->expected_port!=CP_CLOUD_PORT)return CP_CLOUD_PROOF;
 iface=mu34_ap_server_interface(s->airplay);
 if(!iface[0]||s->owner.bonjour_identity_verified(s->owner.ctx,s->epoch,iface)!=1||
    s->owner.airplay_listener_verified(s->owner.ctx,s->epoch,iface,CP_CLOUD_PORT)!=1)
  return CP_CLOUD_RETRY;
 return CP_CLOUD_OK;
}
int cp_cloud_carplay_request(struct cp_cloud_pipeline *s){
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_WIFI_ASSOCIATED)return CP_CLOUD_ORDER;
 /* A test/owner assertion alone can no longer advance the state: the
  * 4301 is emitted by our iAP2 link and must receive its peer ACK. */
 if(!s->bootstrap||!s->bootstrap->wireless_bound||
    !s->bootstrap->wireless.start_sent||s->bootstrap->wireless_pending||
    s->bootstrap->link.outbound_pending||
    !s->bootstrap->wireless.session||
    s->bootstrap->wireless.session->airplay_port!=CP_CLOUD_PORT||
    !s->airplay||s->airplay->generation!=s->epoch||
    mu34_ap_server_state(s->airplay)!=MU34_AP_LISTENING_VERIFIED||
    s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    s->owner.carplay_request_sent(s->owner.ctx,s->epoch)!=1)
  return CP_CLOUD_PROOF;
 return cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_CARPLAY_START_SENT,1)==CP_HO_OK?
   CP_CLOUD_OK:CP_CLOUD_ORDER;
}
int cp_cloud_drive_start(struct cp_cloud_pipeline *s,const void *iface_cf,
                         uint64_t now){
 int r,t;
 if(!s||!s->started||s->stopped)return CP_CLOUD_BAD;
 if(s->handoff.phase>=CP_HO_AIRPLAY_SESSION&&
    s->handoff.phase<=CP_HO_MEDIA_ACTIVE)return CP_CLOUD_OK;
 /* iPhone can ACK 4301 before connecting the IP AirPlay media channel.
  * Subsequent owner ticks must WAIT for the confirmed client session. */
 if(s->handoff.phase==CP_HO_START_REQUESTED){
  if(!eligible(s,1))return stop(s);
  if(!s->airplay||s->airplay->generation!=s->epoch)return CP_CLOUD_PROOF;
  r=cp_cloud_airplay_ready(s,mu34_ap_server_interface(s->airplay));
  return r==CP_CLOUD_PROOF?CP_CLOUD_RETRY:r;
 }
 if(s->handoff.phase!=CP_HO_WIFI_ASSOCIATED)return CP_CLOUD_ORDER;
 if(!s->bootstrap||!s->bootstrap->wireless_bound||
    s->bootstrap->wireless_ctx!=s||
    s->bootstrap->wireless_proof!=actual_wireless_proof)return CP_CLOUD_PROOF;
 r=cp_cloud_prepare_receiver(s,iface_cf);
 if(r!=CP_CLOUD_OK)return r;
 t=cp_s4_tick(s->bootstrap,now);
 if(t==CP_LK_BACKPRESSURE)return CP_CLOUD_RETRY;
 if(t!=CP_LK_OK)return stop(s);
 if(!s->bootstrap->wireless.start_sent||
    s->bootstrap->wireless_pending||s->bootstrap->link.outbound_pending)
  return CP_CLOUD_RETRY;
 r=cp_cloud_carplay_request(s);
 if(r==CP_CLOUD_PROOF)return CP_CLOUD_RETRY;
 if(r!=CP_CLOUD_OK)return r;
 r=cp_cloud_airplay_ready(s,mu34_ap_server_interface(s->airplay));
 return r==CP_CLOUD_PROOF?CP_CLOUD_RETRY:r;
}
static int iface_ok(const char *p){
 size_t i;
 if(!p)return 0;
 for(i=0;i<=CP_CLOUD_IFACE_MAX&&p[i];i++){
  unsigned char c=(unsigned char)p[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
       (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
 }
 return i>0&&i<=CP_CLOUD_IFACE_MAX;
}
int cp_cloud_airplay_ready(struct cp_cloud_pipeline *s,const char *iface){
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_START_REQUESTED)return CP_CLOUD_ORDER;
 if(!iface_ok(iface))return CP_CLOUD_BAD;
 if(!s->airplay||s->airplay->generation!=s->epoch||
    s->airplay->expected_port!=CP_CLOUD_PORT||
    strcmp(mu34_ap_server_interface(s->airplay),iface)!=0||
    mu34_ap_server_poll(s->airplay,s->epoch)!=MU34_AP_R_LISTENING_VERIFIED)
  return CP_CLOUD_PROOF;
 if(s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    s->owner.bonjour_identity_verified(s->owner.ctx,s->epoch,iface)!=1||
    s->owner.airplay_listener_verified(s->owner.ctx,s->epoch,iface,CP_CLOUD_PORT)!=1||
    s->owner.airplay_session_connected(s->owner.ctx,s->epoch,iface,CP_CLOUD_PORT)!=1)
  return CP_CLOUD_PROOF;
 if(cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_AIRPLAY_SESSION_CREATED,1)!=CP_HO_OK)
  return stop(s);
 memcpy(s->iface,iface,strlen(iface)+1);return CP_CLOUD_OK;
}
int cp_cloud_inner_auth(struct cp_cloud_pipeline *s){
 if(!eligible(s,1))return stop(s);
 if(s->handoff.phase!=CP_HO_AIRPLAY_SESSION)return CP_CLOUD_ORDER;
 if(s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    s->owner.inner_iap2_mfi_verified(s->owner.ctx,s->epoch)!=1)return CP_CLOUD_PROOF;
 return cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_IN_SESSION_IAP2_AUTHENTICATED,1)==CP_HO_OK?
  CP_CLOUD_OK:CP_CLOUD_ORDER;
}
int cp_cloud_media_ready(struct cp_cloud_pipeline *s){
 if(!eligible(s,0))return stop(s);
 if(s->handoff.phase!=CP_HO_INNER_IAP_AUTH)return CP_CLOUD_ORDER;
 if(s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    s->owner.inner_iap2_mfi_verified(s->owner.ctx,s->epoch)!=1)return CP_CLOUD_PROOF;
 for(int channel=CP_CLOUD_VIDEO;channel<=CP_CLOUD_STEERING;channel++){
  if(s->owner.media_verified(s->owner.ctx,s->epoch,(enum cp_cloud_media)channel)!=1)
   return CP_CLOUD_PROOF;
 }
 return cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_MEDIA_CONFIRMED,1)==CP_HO_OK?
   CP_CLOUD_OK:CP_CLOUD_ORDER;
}
int cp_cloud_bt_closed(struct cp_cloud_pipeline *s,uint32_t gen){
 if(!s||!s->started||s->stopped)return CP_CLOUD_STOPPED;
 if(gen!=s->bt_generation)return CP_CLOUD_STALE;
 if(s->handoff.phase<CP_HO_INNER_IAP_AUTH)return stop(s);
 if(cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_BT_DISCONNECTED,1)!=CP_HO_OK)
  return CP_CLOUD_ORDER;
 return CP_CLOUD_OK;
}
int cp_cloud_wired_preempt(struct cp_cloud_pipeline *s){
 int restored=CP_CLOUD_OK;
 if(!s)return CP_CLOUD_BAD;
 /* Attempt OEM restore BEFORE giving up the exclusive wireless owner.
  * If it remains pending, revoke wireless now but report incomplete cleanup. */
 if(s->started&&!s->stopped){
  restored=native_restore(s);
  s->stopped=1;
  if(s->radio.epoch)(void)cp_radio_revoke(&s->radio,s->epoch,1);
  if(s->bootstrap)cp_s4_revoke(s->bootstrap);
 }
 if(cp_ho_wired_takeover(&s->handoff,1)!=CP_HO_OK)return CP_CLOUD_ORDER;
 return restored;
}
int cp_cloud_end(struct cp_cloud_pipeline *s){
 int r;
 if(!s||!s->started||!s->epoch)return CP_CLOUD_BAD;
 r=native_restore(s);
 if(r!=CP_CLOUD_OK)return r;
 if(s->owner.teardown_confirmed(s->owner.ctx,s->epoch)!=1)return CP_CLOUD_PROOF;
 if(s->handoff.phase!=CP_HO_TEARDOWN&&s->handoff.phase!=CP_HO_WIRED)
  (void)cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_FAILURE,1);
 if(cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_TEARDOWN_CONFIRMED,1)!=CP_HO_OK)
  return CP_CLOUD_ORDER;
 if(s->radio.epoch){
  if(s->radio.phase!=CP_RADIO_CLOSED)(void)cp_radio_revoke(&s->radio,s->epoch,0);
  if(cp_radio_new_epoch_ready(&s->radio,s->epoch,1)!=CP_RADIO_OK)
   return CP_CLOUD_ORDER;
 }
 s->stopped=1;
 if(s->bootstrap)cp_s4_revoke(s->bootstrap);
 return CP_CLOUD_OK;
}
int cp_cloud_is_active(const struct cp_cloud_pipeline *s){
 if(!s||!s->started||s->stopped||
    s->handoff.phase!=CP_HO_MEDIA_ACTIVE||!cp_ho_media_ready(&s->handoff))return 0;
 if(s->radio.epoch!=s->epoch||s->radio.phase!=CP_RADIO_ASSOCIATED||
    s->radio.selected!=s->band||
    !s->owner.ap_association||
    s->owner.ap_association(s->owner.ctx,s->epoch,s->band)!=1||
    !s->owner.radio_capability||
    s->owner.radio_capability(s->owner.ctx,s->band)!=1)return 0;
 if(!s->owner.wired_idle||s->owner.wired_idle(s->owner.ctx)!=1||
    !s->owner.si_single_owner||s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    !s->owner.inner_iap2_mfi_verified||
    s->owner.inner_iap2_mfi_verified(s->owner.ctx,s->epoch)!=1||
    !s->airplay||s->airplay->generation!=s->epoch||
    mu34_ap_server_state(s->airplay)!=MU34_AP_LISTENING_VERIFIED||
    !s->owner.airplay_listener_verified||
    s->owner.airplay_listener_verified(s->owner.ctx,s->epoch,s->iface,CP_CLOUD_PORT)!=1||
    !s->owner.airplay_session_connected||
    s->owner.airplay_session_connected(s->owner.ctx,s->epoch,s->iface,CP_CLOUD_PORT)!=1||
    !s->owner.bonjour_identity_verified||
    s->owner.bonjour_identity_verified(s->owner.ctx,s->epoch,s->iface)!=1)
  return 0;
 for(int channel=CP_CLOUD_VIDEO;channel<=CP_CLOUD_STEERING;channel++)
  if(s->owner.media_verified(s->owner.ctx,s->epoch,(enum cp_cloud_media)channel)!=1)return 0;
 return 1;
}
