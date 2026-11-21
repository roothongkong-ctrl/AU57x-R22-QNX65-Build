/* GPL-3.0-only. Mirror DiPlay's wireless control *message* transitions.
 * This file does not assert QNX compatibility or implement MFi/authentication.
 */
#include "cp_wireless.h"
#include "cp_event.h"
#include <string.h>
#define WIFI_PRE_MAX 5u
#define WIFI_POST_MAX 2u
#define START_MAX 2u
#define BUFFER_MAX 4096u
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);
}
static int ready(const struct cp_wireless *s) {
    return s && s->authenticated && s->hotspot_ready && s->session && s->send;
}
void cp_wireless_init(struct cp_wireless *s, const struct cp_session *session,
                      cp_send_csm sender,void *sender_context) {
    if (!s) return;
    memset(s,0,sizeof(*s));
    s->session=session; s->send=sender; s->send_ctx=sender_context;
    s->wireless_available=-1;
}
/* A valid param list uses length INCLUDING its own 4-byte header. */
static int locate(const uint8_t *p,size_t n,uint16_t id,
                  const uint8_t **body,size_t *bodylen) {
    size_t off=0;
    int found=0;
    while (off<n) {
        uint16_t len,tag;
        if (n-off<4) return -1;
        len=rd16(p+off);tag=rd16(p+off+2);
        if (len<4 || len>n-off) return -1;
        if (tag==id) {
            if (found) return -1;
            *body=p+off+4;*bodylen=len-4;found=1;
        }
        off+=len;
    }
    return found;
}
int cp_availability_wireless_4300(const uint8_t *f,size_t n,int *value) {
    const uint8_t *group=0,*v=0;
    size_t gl=0,vl=0;
    int k;
    if (!value) return -1;
    *value=-1;
    if (!f || n<6 || n>65535 || rd16(f)!=0x4040 || rd16(f+2)!=n ||
        rd16(f+4)!=0x4300) return -1;
    k=locate(f+6,n-6,1,&group,&gl); /* wireless group */
    if (k<=0) return k;
    k=locate(group,gl,0,&v,&vl); /* available boolean */
    if (k<=0) return k;
    if (vl!=1 || v[0]>1) return -1;
    *value=(int)v[0];
    return 1;
}
static int transmit(struct cp_wireless *s,int start) {
    uint8_t buf[BUFFER_MAX];
    size_t written=0;
    int r;
    if (start) r=cp_encode_start_4301(s->session,buf,sizeof(buf),&written);
    else r=cp_encode_wifi_5703(&s->session->wifi,buf,sizeof(buf),&written);
    if (r!=CP_OK || written==0) return CP_WL_TRANSPORT_ERROR;
    /* Callback must accept exactly one authenticated CSM control frame. */
    if (s->send(s->send_ctx,buf,written)!=0) return CP_WL_TRANSPORT_ERROR;
    return CP_WL_OK;
}
int cp_wireless_receive(struct cp_wireless *s,const uint8_t *f,size_t n) {
    struct cp_event e;
    int state,r;
    if (!s || cp_decode_event(f,n,&e)!=0) return CP_WL_BAD_FRAME;
    if (e.type==CP_EVENT_OTHER) return CP_WL_IGNORED;
    /* Explicitly require the caller to prove OEM MFi auth + live AP identity.
       A readiness flag is an integration assertion, not authentication. */
    if (!ready(s)) {s->blocked_events++;return CP_WL_NOT_READY;}
    switch (e.type) {
        case CP_EVENT_WIFI_REQUEST:
        case CP_EVENT_TRANSPORT_ID:
            if (e.type==CP_EVENT_TRANSPORT_ID) s->transport_notified=1;
            if (s->transport_notified && s->wifi_post_sent>=WIFI_POST_MAX)
                return CP_WL_LIMIT_REACHED;
            if (!s->transport_notified && s->wifi_pre_sent>=WIFI_PRE_MAX)
                return CP_WL_LIMIT_REACHED;
            r=transmit(s,0);
            if (r==CP_WL_OK) {
                if (s->transport_notified) s->wifi_post_sent++;
                else s->wifi_pre_sent++;
            }
            return r;
        case CP_EVENT_CARPLAY_AVAILABLE:
            r=cp_availability_wireless_4300(f,n,&state);
            if (r<0) return CP_WL_BAD_FRAME;
            if (r==0) {s->wireless_available=-1;return CP_WL_PHONE_UNAVAILABLE;}
            s->wireless_available=state;
            if (state!=1) return CP_WL_PHONE_UNAVAILABLE;
            if (!s->airplay_ready) return CP_WL_NOT_READY;
            if (s->start_sent>=START_MAX) return CP_WL_LIMIT_REACHED;
            r=transmit(s,1);
            if (r==CP_WL_OK) s->start_sent++;
            return r;
        case CP_EVENT_WIRELESS_STATUS:
            s->wireless_available=e.available;
            return CP_WL_OK;
        default: return CP_WL_IGNORED;
    }
}
