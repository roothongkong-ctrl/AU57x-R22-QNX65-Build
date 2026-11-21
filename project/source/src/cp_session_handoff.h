/* GPL-3.0-only. Two-session wireless CarPlay lifecycle candidate.
 * Uses upstream MHI2Q handoff ordering as a REFERENCE, not evidence the
 * AU57x hardware/firmware currently supports it.
 * No Bluetooth, MFi, QNX, DIO or AirPlay interaction occurs in this code.
 */
#ifndef CP_SESSION_HANDOFF_H
#define CP_SESSION_HANDOFF_H
#include <stdint.h>

enum cp_ho_phase {
    CP_HO_IDLE=0, CP_HO_BT_LINK=1, CP_HO_BOOTSTRAP_AUTH=2,
    CP_HO_WIFI_ASSOCIATED=3, CP_HO_START_REQUESTED=4,
    CP_HO_AIRPLAY_SESSION=5, CP_HO_INNER_IAP_AUTH=6,
    CP_HO_MEDIA_ACTIVE=7, CP_HO_TEARDOWN=8, CP_HO_WIRED=9
};
enum cp_ho_event {
    CP_HO_EV_BT_CONNECTED=1,
    CP_HO_EV_BOOTSTRAP_MFI_AUTHENTICATED,
    CP_HO_EV_WIFI_AP_AND_PHONE_CONFIRMED,
    CP_HO_EV_CARPLAY_START_SENT,
    CP_HO_EV_AIRPLAY_SESSION_CREATED,
    CP_HO_EV_IN_SESSION_IAP2_AUTHENTICATED,
    CP_HO_EV_MEDIA_CONFIRMED,
    CP_HO_EV_BT_DISCONNECTED,
    CP_HO_EV_AIRPLAY_DISCONNECTED,
    CP_HO_EV_WIRED_STARTED,
    CP_HO_EV_WIRED_ENDED,
    CP_HO_EV_FAILURE,
    CP_HO_EV_TEARDOWN_CONFIRMED
};
enum cp_ho_status {
    CP_HO_OK=0, CP_HO_INVALID=-1, CP_HO_STALE=-2,
    CP_HO_OUT_OF_ORDER=-3, CP_HO_MISSING_OWNER=-4, CP_HO_BLOCKED=-5
};
struct cp_handoff {
    enum cp_ho_phase phase;
    uint32_t epoch;
    uint32_t last_epoch;
    unsigned wired_active:1;
    unsigned bt_connected:1;
    unsigned first_iap_authenticated:1;
    unsigned second_iap_authenticated:1;
    unsigned media_confirmed:1;
};
void cp_ho_init(struct cp_handoff *s);
/* epoch is selected monotonically by the real session owner. No wrap/reuse.
 * This module is NOT that owner, and cannot attest correctness of an event.
 */
int cp_ho_begin(struct cp_handoff *s, uint32_t epoch);
/* owner's evidence=1 asserts this event was verified by actual OEM runtime.
 * A 1 passed from a test is NOT proof of MFi, Apple session, or car support.
 */
int cp_ho_signal(struct cp_handoff *s, uint32_t epoch,
                 enum cp_ho_event ev, int owner_evidence);
/* Independently attested wired priority, including IDLE epoch=0. */
int cp_ho_wired_takeover(struct cp_handoff *s, int owner_confirmed);
int cp_ho_wired_release(struct cp_handoff *s, int owner_confirmed);
int cp_ho_media_ready(const struct cp_handoff *s);
#endif
