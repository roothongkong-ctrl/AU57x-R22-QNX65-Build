#include "cp_session_handoff.h"
#include <string.h>

void cp_ho_init(struct cp_handoff *s) {
    if (s) memset(s, 0, sizeof(*s));
}
int cp_ho_begin(struct cp_handoff *s, uint32_t epoch) {
    if (!s || !epoch) return CP_HO_INVALID;
    if (s->wired_active || s->phase == CP_HO_WIRED) return CP_HO_BLOCKED;
    if (s->phase != CP_HO_IDLE) return CP_HO_OUT_OF_ORDER;
    if (epoch <= s->last_epoch) return CP_HO_STALE;
    s->last_epoch = s->epoch = epoch;
    s->bt_connected=0;
    s->first_iap_authenticated=0;
    s->second_iap_authenticated=0;
    s->media_confirmed=0;
    return CP_HO_OK;
}
static void stop_session(struct cp_handoff *s) {
    s->phase=CP_HO_TEARDOWN;
    s->bt_connected=0;
    s->first_iap_authenticated=0;
    s->second_iap_authenticated=0;
    s->media_confirmed=0;
}
int cp_ho_signal(struct cp_handoff *s, uint32_t epoch,
                 enum cp_ho_event ev, int owner_evidence) {
    if (!s || !epoch || ev < CP_HO_EV_BT_CONNECTED ||
        ev > CP_HO_EV_TEARDOWN_CONFIRMED) return CP_HO_INVALID;
    if (!owner_evidence) return CP_HO_MISSING_OWNER;
    if (epoch != s->epoch) return CP_HO_STALE;
    if (s->wired_active && ev!=CP_HO_EV_WIRED_ENDED &&
        ev!=CP_HO_EV_WIRED_STARTED && ev!=CP_HO_EV_TEARDOWN_CONFIRMED &&
        ev!=CP_HO_EV_FAILURE) return CP_HO_BLOCKED;
    switch (ev) {
    case CP_HO_EV_BT_CONNECTED:
        if (s->phase!=CP_HO_IDLE || s->wired_active) return CP_HO_OUT_OF_ORDER;
        s->bt_connected=1; s->phase=CP_HO_BT_LINK; return CP_HO_OK;
    case CP_HO_EV_BOOTSTRAP_MFI_AUTHENTICATED:
        if (s->phase!=CP_HO_BT_LINK || !s->bt_connected) return CP_HO_OUT_OF_ORDER;
        s->first_iap_authenticated=1; s->phase=CP_HO_BOOTSTRAP_AUTH;return CP_HO_OK;
    case CP_HO_EV_WIFI_AP_AND_PHONE_CONFIRMED:
        if (s->phase!=CP_HO_BOOTSTRAP_AUTH) return CP_HO_OUT_OF_ORDER;
        s->phase=CP_HO_WIFI_ASSOCIATED;return CP_HO_OK;
    case CP_HO_EV_CARPLAY_START_SENT:
        if (s->phase!=CP_HO_WIFI_ASSOCIATED) return CP_HO_OUT_OF_ORDER;
        s->phase=CP_HO_START_REQUESTED;return CP_HO_OK;
    case CP_HO_EV_AIRPLAY_SESSION_CREATED:
        if (s->phase!=CP_HO_START_REQUESTED) return CP_HO_OUT_OF_ORDER;
        s->phase=CP_HO_AIRPLAY_SESSION;return CP_HO_OK;
    case CP_HO_EV_IN_SESSION_IAP2_AUTHENTICATED:
        if (s->phase!=CP_HO_AIRPLAY_SESSION) return CP_HO_OUT_OF_ORDER;
        s->second_iap_authenticated=1; s->phase=CP_HO_INNER_IAP_AUTH;return CP_HO_OK;
    case CP_HO_EV_MEDIA_CONFIRMED:
        if (s->phase!=CP_HO_INNER_IAP_AUTH || !s->second_iap_authenticated)
            return CP_HO_OUT_OF_ORDER;
        s->media_confirmed=1; s->phase=CP_HO_MEDIA_ACTIVE;return CP_HO_OK;
    case CP_HO_EV_BT_DISCONNECTED:
        if (!s->bt_connected) return CP_HO_OUT_OF_ORDER;
        s->bt_connected=0;
        /* BT bootstrap is allowed to drop only AFTER inner iAP2 took over. */
        if (s->phase==CP_HO_INNER_IAP_AUTH || s->phase==CP_HO_MEDIA_ACTIVE)
            return CP_HO_OK;
        stop_session(s);return CP_HO_OK;
    case CP_HO_EV_AIRPLAY_DISCONNECTED:
        if (s->phase!=CP_HO_AIRPLAY_SESSION && s->phase!=CP_HO_INNER_IAP_AUTH &&
            s->phase!=CP_HO_MEDIA_ACTIVE) return CP_HO_OUT_OF_ORDER;
        stop_session(s);return CP_HO_OK;
    case CP_HO_EV_WIRED_STARTED:
        s->wired_active=1;
        if (s->phase==CP_HO_WIRED || s->phase==CP_HO_TEARDOWN) return CP_HO_OK;
        /* Regardless of stage, request teardown and keep wired priority. */
        stop_session(s);return CP_HO_OK;
    case CP_HO_EV_WIRED_ENDED:
        if (!s->wired_active) return CP_HO_OUT_OF_ORDER;
        s->wired_active=0;
        /* A new wireless epoch is allowed ONLY after confirmed teardown. */
        if (s->phase==CP_HO_WIRED) s->phase=CP_HO_IDLE;
        return CP_HO_OK;
    case CP_HO_EV_FAILURE:
        if (s->phase==CP_HO_TEARDOWN) return CP_HO_OK;
        stop_session(s);return CP_HO_OK;
    case CP_HO_EV_TEARDOWN_CONFIRMED:
        if (s->phase!=CP_HO_TEARDOWN) return CP_HO_OUT_OF_ORDER;
        s->phase=s->wired_active ? CP_HO_WIRED : CP_HO_IDLE;
        s->bt_connected=0; s->first_iap_authenticated=0;
        s->second_iap_authenticated=0; s->media_confirmed=0;
        return CP_HO_OK;
    default:return CP_HO_INVALID;
    }
}
int cp_ho_media_ready(const struct cp_handoff *s) {
    return s && s->phase==CP_HO_MEDIA_ACTIVE && !s->wired_active &&
           s->second_iap_authenticated && s->media_confirmed;
}

/* Independent of an existing wireless epoch. This fixes the R7 limitation
 * where a wired session arriving during IDLE could not be recorded because
 * cp_ho_signal() rejected epoch 0. It never implies OEM teardown succeeded.
 */
int cp_ho_wired_takeover(struct cp_handoff *s,int confirmed){
 if(!s||confirmed!=1)return CP_HO_MISSING_OWNER;
 s->wired_active=1;
 if(s->phase==CP_HO_IDLE||s->phase==CP_HO_WIRED){
  s->phase=CP_HO_WIRED;return CP_HO_OK;
 }
 if(s->phase!=CP_HO_TEARDOWN)stop_session(s);
 return CP_HO_OK;
}
int cp_ho_wired_release(struct cp_handoff *s,int confirmed){
 if(!s||confirmed!=1)return CP_HO_MISSING_OWNER;
 if(!s->wired_active)return CP_HO_OUT_OF_ORDER;
 s->wired_active=0;
 if(s->phase==CP_HO_WIRED)s->phase=CP_HO_IDLE;
 return CP_HO_OK;
}
