/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_oem_handoff.h"
#include <string.h>

static int safe_name(const char *s){
 size_t i;
 if(!s)return 0;
 for(i=0;i<16 && s[i];i++){
  const unsigned char c=(unsigned char)s[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
       (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
 }
 return i>0 && i<16;
}
static int same_name(const char *a,const char *b){
 size_t i;
 if(!a||!b)return 0;
 for(i=0;i<17;i++){
  if(a[i]!=b[i])return 0;
  if(a[i]==0)return 1;
 }
 return 0;
}
void cp_oem_handoff_init(struct cp_oem_handoff *h){
 if(!h)return;
 memset(h,0,sizeof *h);
 cp_ho_init(&h->session);
 cp_step2_transport_init(&h->transport);
}
int cp_oem_handoff_begin_mapped(struct cp_oem_handoff *h,
                         struct cp_bt_bridge *bridge,
                         const struct cp_step2_owner *transport_owner,
                         const struct cp_oem_witness *w,
                         uint32_t radio_g,uint32_t local_g,uint32_t epoch){
 int rc;
 if(!h||!bridge||!transport_owner||!w||!w->bt_owner||
    !w->wired_idle||!w->verified_event||!radio_g||!local_g||!epoch)
    return CP_HO_INVALID;
 if(h->bound||h->session.phase!=CP_HO_IDLE)return CP_HO_OUT_OF_ORDER;
 if(w->wired_idle(w->context)!=1||w->bt_owner(w->context,radio_g)!=1)
    return CP_HO_MISSING_OWNER;
 /* Do not assume an R11 wire generation equals a local bridge epoch. */
 if(bridge->generation!=local_g||!bridge->connected||bridge->closed)
    return CP_HO_STALE;
 if(radio_g!=local_g &&
     (!w->bridge_mapping||w->bridge_mapping(w->context,radio_g,local_g)!=1))
    return CP_HO_MISSING_OWNER;
 if(epoch<=h->session.last_epoch)return CP_HO_STALE;
 /* cp_step2_transport speaks the LOCAL generation of this bridge, not the
  * physical BlueSDK generation. Its owner must independently verify both. */
 rc=cp_step2_transport_bind(&h->transport,bridge,transport_owner,local_g);
 if(rc!=CP_BT_OK)return CP_HO_MISSING_OWNER;
 rc=cp_ho_begin(&h->session,epoch);
 if(rc==CP_HO_OK)rc=cp_ho_signal(&h->session,epoch,CP_HO_EV_BT_CONNECTED,1);
 if(rc!=CP_HO_OK){cp_step2_transport_quarantine(&h->transport);return rc;}
 h->witness=*w;h->bridge=bridge;
 h->live_transport=&h->transport;h->borrowed_link=NULL;
 h->physical_generation=radio_g;h->bridge_generation=local_g;
 h->epoch=epoch;h->bound=1;return CP_HO_OK;
}
int cp_oem_handoff_begin(struct cp_oem_handoff *h,
                         struct cp_bt_bridge *bridge,
                         const struct cp_step2_owner *transport_owner,
                         const struct cp_oem_witness *w,
                         uint32_t g,uint32_t epoch){
 return cp_oem_handoff_begin_mapped(h,bridge,transport_owner,w,g,g,epoch);
}
/* Take over the control-state view of an existing R11 IPC connection.
 * The physical RFCOMM RX/TX buffers stay solely with the R11 link. */
int cp_oem_handoff_begin_from_ipc(struct cp_oem_handoff *h,
       struct cp_step2_ipc_link *link,
       const struct cp_oem_witness *w, uint32_t epoch){
 int rc;
 if(!h||!link||!w||!w->wired_idle||!w->bt_owner||
    !w->verified_event||!w->bridge_mapping||!epoch)
    return CP_HO_INVALID;
 if(h->bound||h->session.phase!=CP_HO_IDLE)return CP_HO_OUT_OF_ORDER;
 if(!link->connected||link->poisoned||link->finished||
    !link->radio_generation||!link->local_generation||
    !link->transport.bound||link->transport.bridge!=&link->bridge||
    link->transport.generation!=link->local_generation||
    link->bridge.generation!=link->local_generation||
    link->bridge.closed||!link->bridge.connected)
    return CP_HO_MISSING_OWNER;
 if(!link->owner.owner_alive||
    link->owner.owner_alive(link->owner.opaque,link->peer,link->radio_generation)!=1||
    w->wired_idle(w->context)!=1||
    w->bt_owner(w->context,link->radio_generation)!=1||
    w->bridge_mapping(w->context,link->radio_generation,link->local_generation)!=1)
    return CP_HO_MISSING_OWNER;
 if(epoch<=h->session.last_epoch)return CP_HO_STALE;
 rc=cp_ho_begin(&h->session,epoch);
 if(rc==CP_HO_OK)rc=cp_ho_signal(&h->session,epoch,CP_HO_EV_BT_CONNECTED,1);
 if(rc!=CP_HO_OK)return rc;
 h->witness=*w;h->bridge=&link->bridge;
 h->live_transport=&link->transport;h->borrowed_link=link;
 h->physical_generation=link->radio_generation;
 h->bridge_generation=link->local_generation;
 h->epoch=epoch;h->bound=1;
 return CP_HO_OK;
}
static enum cp_ho_event translate(enum cp_oem_gate_event e){
 switch(e){
 case CP_OEM_BT_IAP_AUTH:return CP_HO_EV_BOOTSTRAP_MFI_AUTHENTICATED;
 case CP_OEM_WIFI_ASSOCIATED:return CP_HO_EV_WIFI_AP_AND_PHONE_CONFIRMED;
 case CP_OEM_CARPLAY_START_SENT:return CP_HO_EV_CARPLAY_START_SENT;
 case CP_OEM_AIRPLAY_SESSION:return CP_HO_EV_AIRPLAY_SESSION_CREATED;
 case CP_OEM_INNER_IAP_AUTH:return CP_HO_EV_IN_SESSION_IAP2_AUTHENTICATED;
 case CP_OEM_DIO_MEDIA:return CP_HO_EV_MEDIA_CONFIRMED;
 default:return 0;
 }
}
int cp_oem_handoff_confirm(struct cp_oem_handoff *h,
                           enum cp_oem_gate_event event,
                           const char *iface){
 enum cp_ho_event he;
 if(!h||!h->bound)return CP_HO_INVALID;
 he=translate(event);
 if(!he)return CP_HO_INVALID;
 if(h->session.wired_active||h->session.phase==CP_HO_TEARDOWN)
  return CP_HO_BLOCKED;
 if(h->witness.wired_idle(h->witness.context)!=1){
  if(h->live_transport)cp_step2_transport_quarantine(h->live_transport);
  (void)cp_ho_signal(&h->session,h->epoch,CP_HO_EV_FAILURE,1);
  return CP_HO_BLOCKED;
 }
 /* Before inner iAP2 takes over, Bluetooth bootstrap ownership must exist.
  * Once AirPlay owns the session, BT can be intentionally disconnected. */
 if(!h->session.second_iap_authenticated &&
     ((h->borrowed_link && (h->borrowed_link->poisoned ||
        !h->borrowed_link->connected || !h->borrowed_link->transport.bound)) ||
      h->witness.bt_owner(h->witness.context,h->physical_generation)!=1 ||
      (h->physical_generation!=h->bridge_generation &&
       (!h->witness.bridge_mapping ||
        h->witness.bridge_mapping(h->witness.context,h->physical_generation,
                                  h->bridge_generation)!=1)))){
  if(h->live_transport)cp_step2_transport_quarantine(h->live_transport);
  (void)cp_ho_signal(&h->session,h->epoch,CP_HO_EV_FAILURE,1);
  return CP_HO_MISSING_OWNER;
 }
 if(event==CP_OEM_WIFI_ASSOCIATED){
  if(!safe_name(iface))return CP_HO_INVALID;
 } else if(iface && iface[0] &&
          !same_name(iface,h->interface_name))return CP_HO_INVALID;
 if(event>=CP_OEM_CARPLAY_START_SENT && !h->interface_name[0])
  return CP_HO_OUT_OF_ORDER;
 /* Runtime witnesses for event 4 must include a real, current AirPlay socket,
  * not merely server-control success. Event 6 must attest DIO frames+controls.
  */
 if(h->witness.verified_event(h->witness.context,event,h->epoch,
      event==CP_OEM_WIFI_ASSOCIATED?iface:h->interface_name)!=1)
  return CP_HO_MISSING_OWNER;
 int rc=cp_ho_signal(&h->session,h->epoch,he,1);
 if(rc==CP_HO_OK && event==CP_OEM_WIFI_ASSOCIATED){
  size_t n=strlen(iface);memcpy(h->interface_name,iface,n+1);
 }
 return rc;
}
int cp_oem_handoff_bt_closed(struct cp_oem_handoff *h,uint32_t gen){
 if(!h||!h->bound)return CP_HO_INVALID;
 if(gen!=h->physical_generation)return CP_HO_STALE;
 /* External close event uses physical BlueSDK generation. The local
  * transport must be closed with the generation it was bound to. */
 if(h->borrowed_link){
    /* R11 must FIRST receive the real RFCOMM disconnect frame. Poison or
     * arbitrary loss must not be misreported as a clean SDK disconnect. */
    if(!h->borrowed_link->clean_disconnect ||
       h->borrowed_link->connected || h->borrowed_link->transport.bound ||
       h->borrowed_link->radio_generation!=h->physical_generation ||
       h->borrowed_link->local_generation!=h->bridge_generation)
       return CP_HO_MISSING_OWNER;
 } else {
    int result=cp_step2_transport_disconnected(&h->transport,h->bridge_generation);
    if(result!=CP_BT_OK)return CP_HO_MISSING_OWNER;
 }
 return cp_ho_signal(&h->session,h->epoch,CP_HO_EV_BT_DISCONNECTED,1);
}
int cp_oem_handoff_wired(struct cp_oem_handoff *h,int active,int confirmed){
 if(!h||confirmed!=1)return CP_HO_MISSING_OWNER;
 if(active){
  if(h->live_transport)cp_step2_transport_quarantine(h->live_transport);
  h->live_transport=NULL;h->borrowed_link=NULL;
  h->bound=0;
  h->physical_generation=0;h->bridge_generation=0;
  h->interface_name[0]=0;
  return cp_ho_wired_takeover(&h->session,1);
 }
 return cp_ho_wired_release(&h->session,1);
}
int cp_oem_handoff_teardown_confirmed(struct cp_oem_handoff *h,int confirmed){
 int rc;
 if(!h||confirmed!=1)return CP_HO_MISSING_OWNER;
 rc=cp_ho_signal(&h->session,h->epoch,CP_HO_EV_TEARDOWN_CONFIRMED,1);
 if(rc==CP_HO_OK){
  if(h->live_transport)cp_step2_transport_quarantine(h->live_transport);
  h->live_transport=NULL;h->borrowed_link=NULL;
  h->bound=0;h->physical_generation=0;h->bridge_generation=0;h->epoch=0;
  h->interface_name[0]=0;
 }
 return rc;
}
int cp_oem_handoff_media_ready(const struct cp_oem_handoff *h){
 return h && h->bound && h->interface_name[0] &&
        cp_ho_media_ready(&h->session) &&
        h->witness.wired_idle && h->witness.wired_idle(h->witness.context)==1 &&
        h->witness.verified_event &&
        h->witness.verified_event(h->witness.context,CP_OEM_DIO_MEDIA,
            h->epoch,h->interface_name)==1;
}
