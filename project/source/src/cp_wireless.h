/* GPL-3.0-only; DiPlay-inspired protocol state coordinator, not an OEM driver. */
#ifndef CP_WIRELESS_H
#define CP_WIRELESS_H
#include "cp_csm.h"
#include <stddef.h>
#include <stdint.h>

/* Callback receives one full CSM frame. Caller must send via authenticated
   iAP2 CONTROL SESSION, not a raw Bluetooth RFCOMM socket. */
typedef int (*cp_send_csm)(void *ctx, const uint8_t *frame, size_t len);

enum cp_wireless_result {
    CP_WL_OK = 0,
    CP_WL_IGNORED = 1,
    CP_WL_NOT_READY = 2,
    CP_WL_LIMIT_REACHED = 3,
    CP_WL_PHONE_UNAVAILABLE = 4,
    CP_WL_BAD_FRAME = -3,
    CP_WL_TRANSPORT_ERROR = -4
};
struct cp_wireless {
    /* supplied and verified by OEM transport owner; NOT inferred by this module */
    int authenticated;
    int hotspot_ready;
    int airplay_ready;
    int transport_notified;
    int wireless_available; /* 0=no, 1=yes, -1=unknown */
    int availability_seen; /* only set by valid phone 4300, not 4E0D */
    unsigned wifi_pre_sent;
    unsigned wifi_post_sent;
    unsigned start_sent;
    unsigned blocked_events;
    const struct cp_session *session; /* caller-owned, current credentials */
    cp_send_csm send;
    void *send_ctx;
};
void cp_wireless_init(struct cp_wireless *state, const struct cp_session *session,
                      cp_send_csm sender, void *sender_context);
/* Reassembled, complete 0x4040 CSM frame, after iAP2 authentication.
   No link frame parsing, MFi handling, Wi-Fi setup, QNX IPC or AirPlay setup. */
int cp_wireless_receive(struct cp_wireless *state,const uint8_t *frame,size_t len);
/* Re-try a phone's valid 4300 after original receiver becomes ready. */
int cp_wireless_start_if_ready(struct cp_wireless *state);
/* Accessory Wi-Fi config is already in cp_csm.c; this finds the optional
   "wireless available" boolean in CarPlayAvailability group 1. */
int cp_availability_wireless_4300(const uint8_t *frame,size_t len,int *value);
#endif
