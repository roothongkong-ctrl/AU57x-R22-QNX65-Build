/* DiPlay -> QNX CSM event inspection. GPL-3.0-only. See LICENSE. */
#ifndef CP_EVENT_H
#define CP_EVENT_H
#include <stddef.h>
#include <stdint.h>
enum cp_event_type {
    CP_EVENT_OTHER=0,
    CP_EVENT_WIFI_REQUEST=1,  /* 0x5702 */
    CP_EVENT_CARPLAY_AVAILABLE=2, /* 0x4300: do not infer available=true */
    CP_EVENT_WIRELESS_STATUS=3, /* 0x4e0d: look at available */
    CP_EVENT_TRANSPORT_ID=4 /* 0x4e0e */
};
struct cp_event {
    uint16_t id;
    enum cp_event_type type;
    int available; /* 0/1 for CP_EVENT_WIRELESS_STATUS; -1 otherwise */
};
/* Complete CSM frame expected. Caller owns link reassembly and authenticated transport.
 * Returns 0 success, -1 invalid arguments, -3 malformed frame or TLVs.
 */
int cp_decode_event(const uint8_t *frame, size_t len, struct cp_event *event);
#endif
