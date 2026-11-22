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
/* Declared here so failure paths on the owner tick can retry OEM teardown,
 * rather than setting stopped forever and orphaning a native SI receiver. */
static int native_failure(struct cp_cloud_pipeline *s);
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
 memset(s,0,sizeof *s);mu_cprx_client_init(&s->rx_client);
 cp_radio_init(&s->radio);cp_ho_init(&s->handoff);
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
static int native_valid(const struct cp_cloud_pipeline *p){
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
/* SI must own this single local v3 receiver session from OPEN onward.
 * Do not accept a controller that someone else has already STARTed: doing so
 * would let this pipeline and the SI socket client both mutate DIO. */
static int native_rx_lease(const struct cp_cloud_pipeline *p,
                           const MuCprxService *rx){
 if(!p||!p->native_dio||!rx||rx->version!=3||
    rx->generation!=p->epoch||rx->epoch!=p->epoch||
    rx->port!=CP_CLOUD_PORT||!rx->session||
    !rx->unix_listener||!rx->unix_path[0]||
    rx->backend.opaque!=p->native_dio||
    rx->backend.generation!=p->epoch||
    !rx->backend.start||!rx->backend.poll||!rx->backend.restore||
    rx->listener<0||rx->terminal||rx->cleaning||rx->backend_released||
    !rx->evidence.admitted||!rx->evidence.si_admitted||
    !rx->evidence.si_client_verified||
    !rx->evidence.identity||!rx->evidence.media||
    !rx->evidence.si_inner_iap2_mfi||!rx->evidence.si_media_channel||
    rx->evidence.admitted(rx->evidence.opaque,rx->generation)!=1||
    rx->evidence.si_admitted(rx->evidence.opaque,rx->generation,
                              rx->epoch,rx->session)!=1)return 0;
 return 1;
}
int cp_cloud_native_rx_live(const struct cp_cloud_pipeline *p,int need_media){
 const MuCprxService *rx=p?p->rx_service:NULL;
 if(!native_valid(p)||!native_rx_lease(p,rx)||
    !rx->started||!rx->start_confirmed)
  return 0;
 if(!need_media)return 1;
 return rx->evidence.media(rx->evidence.opaque)==7;
}
/* Epoch-only status flags can come from a previous iPhone. Require the
 * original SI owner to prove iAP2/MFi and each media/control channel on the
 * SAME CPRX v3 receiver generation, epoch and session ID. */
static int native_inner_live(const struct cp_cloud_pipeline *p){
 const MuCprxService *rx=p?p->rx_service:NULL;
 return cp_cloud_native_rx_live(p,0)&&
  rx->evidence.si_inner_iap2_mfi(rx->evidence.opaque,
       rx->generation,rx->epoch,rx->session)==1;
}
static int native_channel_live(const struct cp_cloud_pipeline *p,
                               enum cp_cloud_media item){
 const MuCprxService *rx=p?p->rx_service:NULL;
 return rx&&item>=CP_CLOUD_VIDEO&&item<=CP_CLOUD_STEERING&&
  native_inner_live(p)&&
  rx->evidence.si_media_channel(rx->evidence.opaque,
       rx->generation,rx->epoch,rx->session,(unsigned)item)==1;
}
int cp_cloud_bind_rx_service(struct cp_cloud_pipeline *p,MuCprxService *rx){
 if(!p||!rx||!p->started||p->stopped||p->rx_service||
    !p->native_dio||!native_valid(p)||
    !native_rx_lease(p,rx)||rx->started||rx->start_confirmed)
  return CP_CLOUD_PROOF;
 p->rx_service=rx;
 return CP_CLOUD_OK;
}
int cp_cloud_enable_native_client(struct cp_cloud_pipeline *p){
 if(!p||!p->started||p->stopped||p->rx_client_enabled||
    p->handoff.phase!=CP_HO_WIFI_ASSOCIATED||!p->rx_service||
    !native_valid(p)||!native_rx_lease(p,p->rx_service)||
    p->rx_service->started||p->rx_service->start_confirmed||
    !eligible(p,1))return CP_CLOUD_PROOF;
 p->rx_client_enabled=1;
 return CP_CLOUD_OK;
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
 int r;
 if(!p||!p->started)return CP_CLOUD_BAD;
 /* An interrupted OEM teardown must continue on later owner ticks, even
  * though wireless has already been marked stopped. Wired priority wins. */
 if(p->stopped){
  if(p->native_dio&&(p->handoff.wired_active||
     !p->owner.wired_idle||p->owner.wired_idle(p->owner.ctx)!=1))
   return cp_cloud_wired_preempt(p);
  if(p->native_cleanup_pending){
   r=cp_cloud_end(p);
   return r==CP_CLOUD_OK?CP_CLOUD_STOPPED:CP_CLOUD_RETRY;
  }
  return CP_CLOUD_STOPPED;
 }
 if(!p->native_dio)return CP_CLOUD_BAD;
 /* A wired connection arriving during a half-sent START or a normal STOP
  * must preempt before any further wireless socket/DIO mutations. */
 if(!p->owner.wired_idle||p->owner.wired_idle(p->owner.ctx)!=1)
  return cp_cloud_wired_preempt(p);
 if(p->graceful_stop_started)return CP_CLOUD_RETRY;
 if(!native_valid(p))return native_failure(p);
 /* The SI CPRX control service is the ONLY native start/stop authority.
  * Tick it on the original owner thread: a real v3 CPRX START first calls
  * mu_dio_start(), and its ACK proves the same receiver was committed. */
 if(!p->rx_service)return CP_CLOUD_PROOF;
 if(!native_rx_lease(p,p->rx_service))return native_failure(p);
 /* R78: the genuine SI owner explicitly opts in. Without an opt-in,
  * retain R77's externally driven protocol. With opt-in, establish one
  * nonblocking START client and require its real AF_UNIX ACK; do not set
  * start_confirmed or simulate inner MFi/media readiness. */
 if(p->rx_client_enabled){
  if(!p->rx_client.phase){
   if(!eligible(p,1)||!p->bootstrap||
      !cp_s4_authenticated(p->bootstrap)||
      p->radio.phase!=CP_RADIO_ASSOCIATED)return CP_CLOUD_RETRY;
   if(mu_cprx_client_begin(&p->rx_client,p->rx_service,now)!=0)
    return native_failure(p);
  }
  if(mu_cprx_client_tick(&p->rx_client,now)==MU_CPRX_CLIENT_ERROR)
   return native_failure(p);
 }
 r=mu_cprx_tick(p->rx_service,now);
 if(r!=0||!native_valid(p))return native_failure(p);
 if(p->rx_client_enabled){
  r=mu_cprx_client_tick(&p->rx_client,now);
  if(r==MU_CPRX_CLIENT_ERROR)return native_failure(p);
  if(r!=MU_CPRX_CLIENT_CONFIRMED)return CP_CLOUD_RETRY;
 }
 if(!cp_cloud_native_rx_live(p,0))return CP_CLOUD_RETRY;
 r=cp_cloud_drive_start(p,p->native_dio->iface,now);
 if(r!=CP_CLOUD_OK)return p->stopped?native_failure(p):r;
 /* A listener and 4301 ACK are not media. Advance the SAME session only
  * after independently observed inner iAP2/MFi authentication, and then
  * after native media==7 and all four OEM delivery/control witnesses. */
 if(p->handoff.phase==CP_HO_AIRPLAY_SESSION){
  r=cp_cloud_inner_auth(p);
  if(r==CP_CLOUD_PROOF)return CP_CLOUD_RETRY;
  if(r!=CP_CLOUD_OK)return p->stopped?native_failure(p):r;
 }
 if(p->handoff.phase==CP_HO_INNER_IAP_AUTH){
  r=cp_cloud_media_ready(p);
  if(r==CP_CLOUD_PROOF)return CP_CLOUD_RETRY;
  if(r!=CP_CLOUD_OK)return p->stopped?native_failure(p):r;
 }
 /* Do not report success on a cached MEDIA_ACTIVE phase after a native
  * stream or SI owner dropped. The next owner tick can trigger cleanup. */
 return p->handoff.phase==CP_HO_MEDIA_ACTIVE&&cp_cloud_is_active(p)?
        CP_CLOUD_OK:CP_CLOUD_RETRY;
}
/* Accept native stop only after the OEM confirms cleanup and original
 * interface restoration; preserve owner for retries otherwise. */
static int native_restore(struct cp_cloud_pipeline *p){
 int r;
 if(!p||!p->native_dio)return CP_CLOUD_OK;
 /* Revoke the sender before releasing the SI receiver lease; the
  * original backend must still acknowledge native restore separately. */
 if(p->rx_client_enabled||p->rx_client.phase){
  mu_cprx_client_close(&p->rx_client);
 }
 p->rx_client_enabled=0;
 p->graceful_stop_started=0;
 /* Release the SI receiver lease before touching the underlying AirPlay
  * singleton. A pending native stop is NOT confirmed cleanup. */
 if(p->rx_service){
  r=mu_cprx_shutdown(p->rx_service);
  if(r!=1||!p->rx_service->backend_released)
   return CP_CLOUD_RETRY;
  p->rx_service=NULL;
 }
 if(!p->native_dio->initialized){
  p->native_dio=0;p->airplay=0;return CP_CLOUD_OK;
 }
 r=mu_dio_restore(p->native_dio);
 if(r==1)return CP_CLOUD_RETRY;
 if(r!=0)return CP_CLOUD_PROOF;
 p->native_dio=0;p->airplay=0;
 return CP_CLOUD_OK;
}
/* Once a genuine SI lease is compromised, never leave native DIO running
 * only because the caller has not explicitly invoked cp_cloud_end(). The
 * rollback lease remains pending until the OEM stop/restore barrier succeeds. */
static int native_failure(struct cp_cloud_pipeline *s){
 int r;
 if(!s)return CP_CLOUD_BAD;
 (void)stop(s);
 s->native_cleanup_pending=1;
 r=cp_cloud_end(s);
 return r==CP_CLOUD_OK?CP_CLOUD_PROOF:CP_CLOUD_RETRY;
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
 /* Native mode must never launch a second server: only the SI's v3
  * CPRX START can call mu_dio_start() for this generation. */
 if(s->native_dio&&!cp_cloud_native_rx_live(s,0))return CP_CLOUD_RETRY;
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
 if(s->native_dio&&!native_inner_live(s))return CP_CLOUD_PROOF;
 if(s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    s->owner.inner_iap2_mfi_verified(s->owner.ctx,s->epoch)!=1)return CP_CLOUD_PROOF;
 return cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_IN_SESSION_IAP2_AUTHENTICATED,1)==CP_HO_OK?
  CP_CLOUD_OK:CP_CLOUD_ORDER;
}
int cp_cloud_media_ready(struct cp_cloud_pipeline *s){
 if(!eligible(s,0))return stop(s);
 if(s->handoff.phase!=CP_HO_INNER_IAP_AUTH)return CP_CLOUD_ORDER;
 /* Media active requires the ORIGINAL receiver to report streaming. */
 if(s->native_dio&&(!cp_cloud_native_rx_live(s,1)||
                   !native_inner_live(s)))return CP_CLOUD_PROOF;
 if(s->owner.si_single_owner(s->owner.ctx,s->epoch)!=1||
    s->owner.inner_iap2_mfi_verified(s->owner.ctx,s->epoch)!=1)return CP_CLOUD_PROOF;
 for(int channel=CP_CLOUD_VIDEO;channel<=CP_CLOUD_STEERING;channel++){
  if(s->native_dio&&!native_channel_live(s,(enum cp_cloud_media)channel))
   return CP_CLOUD_PROOF;
  if(s->owner.media_verified(s->owner.ctx,s->epoch,(enum cp_cloud_media)channel)!=1)
   return CP_CLOUD_PROOF;
 }
 return cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_MEDIA_CONFIRMED,1)==CP_HO_OK?
   CP_CLOUD_OK:CP_CLOUD_ORDER;
}
int cp_cloud_bt_closed(struct cp_cloud_pipeline *s,uint32_t gen){
 if(!s||!s->started||s->stopped)return CP_CLOUD_STOPPED;
 if(gen!=s->bt_generation)return CP_CLOUD_STALE;
 if(s->handoff.phase<CP_HO_INNER_IAP_AUTH)
  return s->native_dio?native_failure(s):stop(s);
 if(cp_ho_signal(&s->handoff,s->epoch,CP_HO_EV_BT_DISCONNECTED,1)!=CP_HO_OK)
  return CP_CLOUD_ORDER;
 return CP_CLOUD_OK;
}
int cp_cloud_graceful_native_end(struct cp_cloud_pipeline *s,uint64_t now){
 int r,t;
 if(!s||!s->started||!s->native_dio||!s->rx_service)
  return CP_CLOUD_BAD;
 if(s->stopped)return s->native_cleanup_pending?CP_CLOUD_RETRY:CP_CLOUD_STOPPED;
 /* Wired takeover must interrupt an in-flight voluntary STOP, not wait for
  * media idle or an ACK from a wireless session that is being displaced. */
 if(!s->owner.wired_idle||s->owner.wired_idle(s->owner.ctx)!=1)
  return cp_cloud_wired_preempt(s);
 if(!s->graceful_stop_started){
  /* Normal disconnect is voluntary: do not cut off actively streaming
   * native media. For immediate wired priority, call wired_preempt instead. */
  if(!native_valid(s)||!native_rx_lease(s,s->rx_service))
   return native_failure(s);
  if(!s->rx_service->started||!s->rx_service->start_confirmed||
     s->rx_service->evidence.media(s->rx_service->evidence.opaque)==7)
   return CP_CLOUD_RETRY;
  if(s->rx_client_enabled&&s->rx_client.phase!=4)
   return CP_CLOUD_RETRY;
  mu_cprx_client_close(&s->rx_client);
  s->rx_client_enabled=0;
  if(mu_cprx_client_begin_stop(&s->rx_client,s->rx_service,now)!=0)
   return CP_CLOUD_PROOF;
  s->graceful_stop_started=1;
 }
 r=mu_cprx_client_tick(&s->rx_client,now);
 if(r==MU_CPRX_CLIENT_ERROR)return native_failure(s);
 if(r==MU_CPRX_CLIENT_BUSY){
  mu_cprx_client_close(&s->rx_client);
  s->graceful_stop_started=0;
  return CP_CLOUD_RETRY;
 }
 t=mu_cprx_tick(s->rx_service,now);
 if(t<0)return native_failure(s);
 if(r!=MU_CPRX_CLIENT_CONFIRMED)
  r=mu_cprx_client_tick(&s->rx_client,now);
 if(r==MU_CPRX_CLIENT_ERROR)return native_failure(s);
 if(r==MU_CPRX_CLIENT_BUSY){
  mu_cprx_client_close(&s->rx_client);
  s->graceful_stop_started=0;
  return CP_CLOUD_RETRY;
 }
 if(r!=MU_CPRX_CLIENT_CONFIRMED)return CP_CLOUD_RETRY;
 /* Service may have been terminated after emitting the last authenticated
  * ACK; that is expected only if it already finished OEM restoration. */
 if(!s->rx_service->backend_released)return native_failure(s);
 mu_cprx_client_close(&s->rx_client);
 s->graceful_stop_started=0;
 return cp_cloud_end(s);
}
int cp_cloud_wired_preempt(struct cp_cloud_pipeline *s){
 int restored;
 if(!s)return CP_CLOUD_BAD;
 /* Revoke wireless control on first wired takeover. Do NOT report a
  * completed native restore until mu_cprx_shutdown() and the original
  * interface barrier really finish. Subsequent calls must retry even
  * after the wireless session has already been marked stopped. */
 if(s->started&&!s->stopped){
  s->stopped=1;
  if(s->radio.epoch)(void)cp_radio_revoke(&s->radio,s->epoch,1);
  if(s->bootstrap)cp_s4_revoke(s->bootstrap);
 }
 if(cp_ho_wired_takeover(&s->handoff,1)!=CP_HO_OK)return CP_CLOUD_ORDER;
 restored=native_restore(s);
 if(restored==CP_CLOUD_OK)s->native_cleanup_pending=0;
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
 s->native_cleanup_pending=0;
 if(s->bootstrap)cp_s4_revoke(s->bootstrap);
 return CP_CLOUD_OK;
}
int cp_cloud_is_active(const struct cp_cloud_pipeline *s){
 if(!s||!s->started||s->stopped||s->graceful_stop_started||
    s->handoff.phase!=CP_HO_MEDIA_ACTIVE||!cp_ho_media_ready(&s->handoff))return 0;
 if(s->native_dio&&(!cp_cloud_native_rx_live(s,1)||
                   !native_inner_live(s)))return 0;
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
 for(int channel=CP_CLOUD_VIDEO;channel<=CP_CLOUD_STEERING;channel++){
  if(s->native_dio&&!native_channel_live(s,(enum cp_cloud_media)channel))return 0;
  if(s->owner.media_verified(s->owner.ctx,s->epoch,(enum cp_cloud_media)channel)!=1)return 0;
 }
 return 1;
}
