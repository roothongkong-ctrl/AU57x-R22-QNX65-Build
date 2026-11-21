/* QNX-independent host model test. Not a real OEM session, auth or radio. */
#include "cp_session_handoff.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 struct cp_handoff s;
 cp_ho_init(&s);
 assert(!cp_ho_media_ready(&s));
 assert(cp_ho_begin(&s,0)==CP_HO_INVALID);
 assert(cp_ho_begin(&s,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_BT_CONNECTED,0)==CP_HO_MISSING_OWNER);
 assert(cp_ho_signal(&s,1,CP_HO_EV_BOOTSTRAP_MFI_AUTHENTICATED,1)==CP_HO_OUT_OF_ORDER);
 assert(cp_ho_signal(&s,1,CP_HO_EV_BT_CONNECTED,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_CARPLAY_START_SENT,1)==CP_HO_OUT_OF_ORDER);
 assert(cp_ho_signal(&s,1,CP_HO_EV_BOOTSTRAP_MFI_AUTHENTICATED,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_WIFI_AP_AND_PHONE_CONFIRMED,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_CARPLAY_START_SENT,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_AIRPLAY_SESSION_CREATED,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_IN_SESSION_IAP2_AUTHENTICATED,1)==CP_HO_OK);
 assert(!cp_ho_media_ready(&s));
 assert(cp_ho_signal(&s,1,CP_HO_EV_MEDIA_CONFIRMED,1)==CP_HO_OK);
 assert(cp_ho_media_ready(&s));
 assert(cp_ho_signal(&s,1,CP_HO_EV_BT_DISCONNECTED,1)==CP_HO_OK);
 assert(cp_ho_media_ready(&s)); /* BT bootstrap optional after inner iAP2 */
 assert(cp_ho_wired_takeover(&s,0)==CP_HO_MISSING_OWNER);
 assert(cp_ho_wired_takeover(&s,1)==CP_HO_OK);
 assert(!cp_ho_media_ready(&s));
 assert(cp_ho_begin(&s,2)==CP_HO_BLOCKED);
 assert(cp_ho_signal(&s,1,CP_HO_EV_TEARDOWN_CONFIRMED,1)==CP_HO_OK);
 assert(cp_ho_wired_release(&s,1)==CP_HO_OK);
 assert(cp_ho_begin(&s,1)==CP_HO_STALE);
 assert(cp_ho_begin(&s,2)==CP_HO_OK);
 assert(cp_ho_signal(&s,1,CP_HO_EV_BT_CONNECTED,1)==CP_HO_STALE);
 assert(cp_ho_signal(&s,2,CP_HO_EV_BT_CONNECTED,1)==CP_HO_OK);
 assert(cp_ho_signal(&s,2,CP_HO_EV_BT_DISCONNECTED,1)==CP_HO_OK);
 assert(s.phase==CP_HO_TEARDOWN);
 assert(!cp_ho_media_ready(&s));
 assert(cp_ho_signal(&s,2,CP_HO_EV_TEARDOWN_CONFIRMED,1)==CP_HO_OK);
 assert(cp_ho_wired_takeover(&s,1)==CP_HO_OK);
 assert(cp_ho_begin(&s,3)==CP_HO_BLOCKED);
 assert(cp_ho_wired_release(&s,1)==CP_HO_OK);
 assert(cp_ho_begin(&s,3)==CP_HO_OK);
 puts("AU57X_CLOUD12_HANDOFF_LIFECYCLE_PASS");
 return 0;
}
