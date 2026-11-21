/* Read-only parser for DiPlay wireless iAP2 control-event envelope. GPL-3.0-only. */
#include "cp_event.h"
static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
int cp_decode_event(const uint8_t *frame, size_t len, struct cp_event *out) {
    size_t pos;
    int availability_found = 0;
    if (!frame || !out) return -1;
    out->id = 0; out->type = CP_EVENT_OTHER; out->available = -1;
    if (len < 6 || len > 65535 || frame[0] != 0x40 || frame[1] != 0x40 ||
        be16(frame+2) != len) return -3;
    out->id = be16(frame+4);
    switch (out->id) {
        case 0x5702: out->type=CP_EVENT_WIFI_REQUEST;break;
        case 0x4300: out->type=CP_EVENT_CARPLAY_AVAILABLE;break;
        case 0x4e0d: out->type=CP_EVENT_WIRELESS_STATUS;break;
        case 0x4e0e: out->type=CP_EVENT_TRANSPORT_ID;break;
        default: break;
    }
    pos = 6;
    while (pos < len) {
        uint16_t plen, pid;
        if (len-pos < 4) return -3;
        plen = be16(frame+pos); pid = be16(frame+pos+2);
        if (plen < 4 || plen > len-pos) return -3;
        if (out->type == CP_EVENT_WIRELESS_STATUS && pid == 0) {
            uint8_t value;
            if (availability_found || plen != 5) return -3;
            value=frame[pos+4];
            if (value != 0 && value != 1) return -3;
            out->available=value; availability_found=1;
        }
        pos+=plen;
    }
    if (out->type == CP_EVENT_WIRELESS_STATUS && !availability_found) return -3;
    return 0;
}
