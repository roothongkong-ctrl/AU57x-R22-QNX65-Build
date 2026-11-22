/* Derived from DiPlay iAP2 messages, see AUTHORS.md and LICENSE. GPL-3.0-only. */
#include "cp_csm.h"
#include <string.h>

#define CP_CSM_MAX ((size_t)65535)
struct writer { uint8_t *bytes; size_t len, cap; int error; };

static void put8(struct writer *w, uint8_t x) {
    if (w->error) return;
    if (w->len >= w->cap || w->len >= CP_CSM_MAX) { w->error = CP_OVERFLOW; return; }
    w->bytes[w->len++] = x;
}
static void put16(struct writer *w, uint16_t x) {
    put8(w, (uint8_t)(x >> 8)); put8(w, (uint8_t)x);
}
static void put32(struct writer *w, uint32_t x) {
    put16(w, (uint16_t)(x >> 16)); put16(w, (uint16_t)x);
}
static void putmem(struct writer *w, const uint8_t *p, size_t n) {
    if (w->error) return;
    if (!p || n > w->cap - w->len || n > CP_CSM_MAX - w->len) {
        w->error = CP_OVERFLOW; return;
    }
    memcpy(w->bytes + w->len, p, n); w->len += n;
}
static size_t beginparam(struct writer *w, uint16_t id) {
    size_t start = w->len;
    put16(w, 0); put16(w, id);
    return start;
}
static void endparam(struct writer *w, size_t start) {
    size_t n;
    if (w->error) return;
    n = w->len - start;
    if (n > 65535) { w->error = CP_OVERFLOW; return; }
    w->bytes[start] = (uint8_t)(n >> 8);
    w->bytes[start + 1] = (uint8_t)n;
}
static void param_bytes(struct writer *w, uint16_t id, const uint8_t *p, size_t n) {
    size_t start = beginparam(w,id); putmem(w,p,n); endparam(w,start);
}
static void param_u8(struct writer *w, uint16_t id, uint8_t x) {
    size_t start = beginparam(w,id); put8(w,x); endparam(w,start);
}
static void param_u16(struct writer *w,uint16_t id,uint16_t x){
    size_t start=beginparam(w,id);put16(w,x);endparam(w,start);
}
static void param_flag(struct writer *w,uint16_t id){
    size_t start=beginparam(w,id);endparam(w,start);
}
static void param_ids(struct writer *w,uint16_t id,const uint16_t *ids,size_t count){
    size_t start=beginparam(w,id);
    for(size_t i=0;i<count;i++)put16(w,ids[i]);
    endparam(w,start);
}
static void param_u32(struct writer *w, uint16_t id, uint32_t x) {
    size_t start = beginparam(w,id); put32(w,x); endparam(w,start);
}
static void param_string(struct writer *w, uint16_t id, const char *s) {
    size_t start, n;
    if (!s || strlen(s) >= CP_CSM_MAX) { w->error = CP_BAD_ARGS; return; }
    start = beginparam(w,id); n = strlen(s)+1;
    putmem(w,(const uint8_t *)s,n); endparam(w,start);
}
static void frame_start(struct writer *w, uint16_t id) {
    put16(w,0x4040); put16(w,0); put16(w,id);
}
static void frame_finish(struct writer *w, size_t *written) {
    if (w->error) return;
    if (w->len > CP_CSM_MAX || w->len < 6) { w->error = CP_OVERFLOW; return; }
    w->bytes[2] = (uint8_t)(w->len >> 8);
    w->bytes[3] = (uint8_t)w->len;
    *written = w->len;
}
static int valid_wifi(const struct cp_wifi *wifi) {
    if (!wifi || !wifi->ssid || !wifi->passphrase || !wifi->ssid[0] || wifi->security > 4) return 0;
    if (wifi->security != 0 && !wifi->passphrase[0]) return 0;
    return 1;
}
int cp_encode_wifi_5703(const struct cp_wifi *wifi,
                        uint8_t *output, size_t capacity, size_t *written) {
    struct writer w = { output, 0, capacity, 0 };
    if (written) *written = 0;
    if (!valid_wifi(wifi) || !output || !written || capacity < 6) return CP_BAD_ARGS;
    frame_start(&w,0x5703);
    if (wifi->bssid6) param_bytes(&w,0,wifi->bssid6,6);
    param_string(&w,1,wifi->ssid);
    param_string(&w,2,wifi->passphrase);
    param_u8(&w,3,wifi->security);
    param_u8(&w,4,wifi->channel);
    frame_finish(&w,written);
    return w.error;
}
static int nonempty(const char *s){return s&&s[0];}
int cp_encode_identification_1d01(const struct cp_identification *id,
                                  uint8_t *out,size_t cap,size_t *written){
 struct writer w={out,0,cap,0};
 size_t group;
 if(written)*written=0;
 if(!id||!out||!written||cap<6||
    !nonempty(id->name)||!nonempty(id->model)||!nonempty(id->manufacturer)||
    !nonempty(id->serial)||!nonempty(id->firmware)||!nonempty(id->hardware)||
    !nonempty(id->bluetooth_name)||!nonempty(id->wireless_ssid)||
    !id->bluetooth_mac6||!id->accessory_messages||!id->phone_messages||
    !id->accessory_message_count||!id->phone_message_count||
    id->accessory_message_count>128||id->phone_message_count>128)
     return CP_BAD_ARGS;
 frame_start(&w,0x1d01);
 param_string(&w,0,id->name);
 param_string(&w,1,id->model);
 param_string(&w,2,id->manufacturer);
 param_string(&w,3,id->serial);
 param_string(&w,4,id->firmware);
 param_string(&w,5,id->hardware);
 param_ids(&w,6,id->accessory_messages,id->accessory_message_count);
 param_ids(&w,7,id->phone_messages,id->phone_message_count);
 param_u8(&w,8,0); /* No external power supply */
 param_u16(&w,9,0);
 param_string(&w,12,"en");
 param_string(&w,13,"en");
 /* Bluetooth iAP2 transport (subclass overrides id3 and repeats name4). */
 group=beginparam(&w,17);
 param_u16(&w,0,0);
 param_string(&w,1,id->bluetooth_name);
 param_flag(&w,2);
 param_bytes(&w,3,id->bluetooth_mac6,6);
 param_string(&w,4,id->bluetooth_name);
 param_flag(&w,5);
 endparam(&w,group);
 /* Wireless CarPlay advertised on the actual same SSID as 0x5703. */
 group=beginparam(&w,24);
 param_u16(&w,0,1);
 param_string(&w,1,id->wireless_ssid);
 param_flag(&w,2);
 param_u16(&w,3,1);
 param_flag(&w,4);
 param_flag(&w,5);
 endparam(&w,group);
 frame_finish(&w,written);
 return w.error;
}
int cp_encode_start_4301(const struct cp_session *s,
                         uint8_t *output, size_t capacity, size_t *written) {
    struct writer w = { output, 0, capacity, 0 };
    size_t group, i;
    if (written) *written = 0;
    if (!s || !output || !written || !valid_wifi(&s->wifi) ||
        !s->airplay_port || !s->public_key || !s->public_key[0] ||
        !s->source_version || !s->source_version[0] ||
        !s->ip_addresses || !s->ip_address_count || s->ip_address_count > 32 ||
        capacity < 6) return CP_BAD_ARGS;
    for (i=0;i<s->ip_address_count;i++)
        if (!s->ip_addresses[i] || !s->ip_addresses[i][0]) return CP_BAD_ARGS;
    frame_start(&w,0x4301);
    group = beginparam(&w,1);
    param_string(&w,0,s->wifi.ssid);
    param_string(&w,1,s->wifi.passphrase);
    param_u8(&w,2,s->wifi.channel);
    for (i=0;i<s->ip_address_count;i++) param_string(&w,3,s->ip_addresses[i]);
    param_u8(&w,4,s->wifi.security);
    endparam(&w,group);
    param_u32(&w,2,s->airplay_port);
    if (s->device_identifier) param_string(&w,3,s->device_identifier);
    param_string(&w,4,s->public_key);
    param_string(&w,5,s->source_version);
    frame_finish(&w,written);
    return w.error;
}
