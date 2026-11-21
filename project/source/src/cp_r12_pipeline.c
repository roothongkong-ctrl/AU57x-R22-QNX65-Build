/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_r12_pipeline.h"
#include <string.h>
static void evidence_lost(struct cp_r12_pipeline *p);
static int base(struct cp_r12_pipeline *p){
    if (!p||!p->attached||!p->handoff||!p->dio.initialized||p->stopping)
        return CP_R12_ORDER;
    if (p->epoch!=p->handoff->epoch) return CP_R12_STALE;
    if (!p->handoff->bound||p->handoff->session.wired_active||
        p->evidence.wired_idle(p->evidence.opaque)!=1) return CP_R12_WIRED;
    if (p->evidence.wifi_peer_live(p->evidence.opaque,p->epoch,
                                   p->handoff->interface_name)!=1){
        /* R21: no reauthorization if peer reconnects in this epoch. */
        evidence_lost(p);
        return CP_R12_OWNER;
    }
    return CP_R12_OK;
}
static int same(const struct cp_r12_pipeline *p,uint64_t tag){
    return tag && p->evidence.same_phone(p->evidence.opaque,p->epoch,tag)==1;
}
/* R20: if an already-verified OEM listener disappears, this wireless epoch
 * must not regain media permission simply because a socket starts listening
 * again. The new listener may represent a DIFFERENT AirPlay/iAP2 session.
 * Preserve the OEM stop/restore lease so cleanup remains possible.
 * Before the first verified listener, normal pending->listening is allowed. */
static void evidence_lost(struct cp_r12_pipeline *p){
    if(!p || !p->attached)return;
    /* A pending first start also needs its original server stop; a later
     * successful peer/attestation must not revive the old session. */
    if(!p->start_attempted && !p->listener_verified &&
       !p->session_verified && !p->inner_verified && !p->media_verified)
        return;
    p->listener_verified=0;
    p->session_verified=0;
    p->inner_verified=0;
    p->media_verified=0;
    p->phone_tag=0;
    p->stopping=1;
}
static void listener_lost(struct cp_r12_pipeline *p){
    if(!p || !p->listener_verified)return;
    evidence_lost(p); /* only cp_r12_stop may restore/release this epoch */
}
void cp_r12_init(struct cp_r12_pipeline *p){if(p)memset(p,0,sizeof *p);}
/* Only after native stop + interface restoration have been positively
 * confirmed may the same long-lived owner-side pipeline be reused. A pending
 * restore deliberately retains every borrowed OEM pointer and the stop latch.
 * Do not memset(p) here: the owner may still use the object on its thread. */
static void completed_restore(struct cp_r12_pipeline *p){
    p->attached=0;
    p->start_attempted=0;
    p->listener_verified=0;
    p->session_verified=0;
    p->inner_verified=0;
    p->media_verified=0;
    p->phone_tag=0;
    p->port=0;
    p->epoch=0;
    p->handoff=NULL;
    memset(&p->evidence,0,sizeof p->evidence);
    p->stopping=0;
    p->wire_preempt_pending=0;
}

int cp_r12_attach(struct cp_r12_pipeline *p,struct cp_oem_handoff *h,
                  const struct cp_r12_evidence *ev,void *lib,
                  void *server,const void *manager,const MuDioGuard *guard,
                  const char *original_iface,unsigned port){
    if(!p||!h||!ev||!guard||!original_iface||!port||port>65535||
       !ev->wired_idle||!ev->wifi_peer_live||!ev->same_phone||
       !ev->airplay_session_live||!ev->inner_iap_authenticated||
       !ev->dio_frames_and_controls||p->attached||p->dio.initialized||
       !h->bound||!h->epoch||!h->interface_name[0]||
       h->session.phase!=CP_HO_WIFI_ASSOCIATED)return CP_R12_INVALID;
    if(ev->wired_idle(ev->opaque)!=1||
       ev->wifi_peer_live(ev->opaque,h->epoch,h->interface_name)!=1)
        return CP_R12_OWNER;
    /* This is an *existing* singleton, not a new server or an OEM patch. */
    if(mu_dio_bind_interface(&p->dio,lib,server,manager,h->epoch,guard,
                             original_iface,h->interface_name)!=0)
        return CP_R12_NATIVE;
    /* An earlier, fully restored wireless epoch must not carry over start
     * or identity flags. Cleanup is never permitted while native is pending. */
    p->handoff=h;p->evidence=*ev;p->epoch=h->epoch;p->port=port;
    p->start_attempted=0;p->listener_verified=0;p->session_verified=0;
    p->inner_verified=0;p->media_verified=0;p->phone_tag=0;
    p->stopping=0;p->wire_preempt_pending=0;p->attached=1;
    return CP_R12_OK;
}
int cp_r12_start(struct cp_r12_pipeline *p){
    int rc=base(p);
    if(rc)return rc;
    if(p->start_attempted||p->handoff->session.phase!=CP_HO_WIFI_ASSOCIATED)
        return CP_R12_ORDER;
    /* This must be an independently observed authenticated control-session
       transmission, not a command this adapter pretends to have sent. */
    rc=cp_oem_handoff_confirm(p->handoff,CP_OEM_CARPLAY_START_SENT,NULL);
    if(rc!=CP_HO_OK)return CP_R12_OWNER;
    p->start_attempted=1; /* preserve restore obligation on failed vendor call */
    rc=mu_dio_start(&p->dio,p->port);
    if(rc<0){
        /* R22: vendor may have partially applied the start command even
         * when returning an error. Never allow a later poll to revive this
         * session; require explicit native stop/interface restoration. */
        evidence_lost(p);
        return CP_R12_NATIVE;
    }
    if(rc==2){p->listener_verified=1;return CP_R12_LISTENING;}
    return CP_R12_PENDING;
}
int cp_r12_poll_listener(struct cp_r12_pipeline *p){
    int rc=base(p);
    if(rc)return rc;
    if(!p->start_attempted)return CP_R12_ORDER;
    rc=mu_dio_poll(&p->dio);
    if(rc==2){p->listener_verified=1;return CP_R12_LISTENING;}
    if(p->listener_verified){
        listener_lost(p);
        return CP_R12_STALE; /* no automatic re-authorization of old iAP2 */
    }
    if(rc!=1){
        /* R22: an unknown/failed OEM listener probe is not a pending state.
         * Do not accept a subsequent replacement socket in the same epoch. */
        evidence_lost(p);
        return CP_R12_NATIVE;
    }
    return CP_R12_PENDING;
}
int cp_r12_observe_airplay(struct cp_r12_pipeline *p,uint64_t tag){
    int rc=base(p);
    if(rc)return rc;
    if(p->session_verified||!p->start_attempted||
       p->handoff->session.phase!=CP_HO_START_REQUESTED)return CP_R12_ORDER;
    if(cp_r12_poll_listener(p)!=CP_R12_LISTENING)return CP_R12_ORDER;
    if(!same(p,tag)||p->evidence.airplay_session_live(p->evidence.opaque,p->epoch,tag)!=1)
        return CP_R12_OWNER;
    if(cp_oem_handoff_confirm(p->handoff,CP_OEM_AIRPLAY_SESSION,NULL)!=CP_HO_OK)
        return CP_R12_OWNER;
    p->phone_tag=tag;p->session_verified=1;return CP_R12_OK;
}
int cp_r12_observe_inner_iap2(struct cp_r12_pipeline *p,uint64_t tag){
    int rc=base(p);
    if(rc)return rc;
    if(!p->session_verified||p->inner_verified||p->phone_tag!=tag||
       p->handoff->session.phase!=CP_HO_AIRPLAY_SESSION)return CP_R12_ORDER;
    if(!same(p,tag)||p->evidence.airplay_session_live(p->evidence.opaque,p->epoch,tag)!=1||
       p->evidence.inner_iap_authenticated(p->evidence.opaque,p->epoch,tag)!=1)
        return CP_R12_OWNER;
    if(cp_oem_handoff_confirm(p->handoff,CP_OEM_INNER_IAP_AUTH,NULL)!=CP_HO_OK)
        return CP_R12_OWNER;
    p->inner_verified=1;return CP_R12_OK;
}
int cp_r12_observe_dio_media(struct cp_r12_pipeline *p,uint64_t tag){
    int rc=base(p);
    if(rc)return rc;
    if(!p->inner_verified||p->media_verified||tag!=p->phone_tag||
       p->handoff->session.phase!=CP_HO_INNER_IAP_AUTH)return CP_R12_ORDER;
    if(!same(p,tag)||p->evidence.dio_frames_and_controls(p->evidence.opaque,p->epoch,tag)!=1)
        return CP_R12_OWNER;
    if(cp_oem_handoff_confirm(p->handoff,CP_OEM_DIO_MEDIA,NULL)!=CP_HO_OK)
        return CP_R12_OWNER;
    p->media_verified=1;return CP_R12_OK;
}
int cp_r12_media_ready(struct cp_r12_pipeline *p){
    if(base(p)||!p->media_verified||!p->inner_verified||!p->session_verified||
       !p->phone_tag||!p->listener_verified)return 0;
    /* R21: do not let transient loss of independent identity, AirPlay,
     * inner-iAP2 or DIO evidence silently reauthorize the old epoch. */
    if(p->evidence.same_phone(p->evidence.opaque,p->epoch,p->phone_tag)!=1||
       p->evidence.airplay_session_live(p->evidence.opaque,p->epoch,p->phone_tag)!=1||
       p->evidence.inner_iap_authenticated(p->evidence.opaque,p->epoch,p->phone_tag)!=1||
       p->evidence.dio_frames_and_controls(p->evidence.opaque,p->epoch,p->phone_tag)!=1){
        evidence_lost(p);
        return 0;
    }
    /* Recheck native listener and the original controller's proof, not a
       cached boolean produced by the preceding commands. */
    if(mu_dio_poll(&p->dio)!=2){listener_lost(p);return 0;}
    return cp_oem_handoff_media_ready(p->handoff)==1;
}
int cp_r12_stop(struct cp_r12_pipeline *p){
    int rc;
    if(!p)return CP_R12_INVALID;
    p->stopping=1;p->media_verified=0;p->inner_verified=0;
    p->listener_verified=0;p->session_verified=0;p->phone_tag=0;
    if(!p->dio.initialized){
        if(p->wire_preempt_pending && p->handoff &&
           cp_oem_handoff_wired(p->handoff,1,1)!=CP_HO_OK)
            return CP_R12_OWNER;
        completed_restore(p);return CP_R12_OK;
    }
    rc=mu_dio_restore(&p->dio);
    if(rc==0){
        if(p->wire_preempt_pending && p->handoff &&
           cp_oem_handoff_wired(p->handoff,1,1)!=CP_HO_OK)
            return CP_R12_OWNER;
        completed_restore(p);return CP_R12_OK;
    }
    return rc==1?CP_R12_PENDING:CP_R12_NATIVE;
}
/* Wired priority: revoke local media synchronously and request native
 * restoration while the original owner lease is still valid. If restoration
 * is PENDING/FAILED, mark wired takeover in the state machine but PRESERVE
 * the borrowed OEM lease for the next retry. Native owner revocation is
 * deferred until cp_r12_stop() observes fully restored state. No new media
 * is permitted while this latch is set. */
int cp_r12_wired_preempt(struct cp_r12_pipeline *p,int owner_confirmed){
    int rc;
    if(!p||!p->handoff||owner_confirmed!=1)return CP_R12_OWNER;
    p->wire_preempt_pending=1;
    p->stopping=1;p->media_verified=0;
    rc=cp_r12_stop(p);
    if(rc==CP_R12_OK)return CP_R12_OK;
    /* Must not call cp_oem_handoff_wired() yet: it destroys the cleanup
     * lease. This records wired priority without invalidating live pointers. */
    if(!p->handoff || cp_ho_wired_takeover(&p->handoff->session,1)!=CP_HO_OK)
        return CP_R12_OWNER;
    return rc;
}
