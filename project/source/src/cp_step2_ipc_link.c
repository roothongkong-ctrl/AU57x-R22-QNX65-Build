/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_step2_ipc_link.h"
#include <string.h>

static uint32_t read32(const uint8_t *p){
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p,uint32_t v){
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static int poison(struct cp_step2_ipc_link *s){
    if (!s) return CP_BT_BAD_ARG;
    s->poisoned=1; s->connected=0; s->finished=1; s->clean_disconnect=0;
    s->used=0;s->expected=8;
    cp_step2_transport_quarantine(&s->transport);
    cp_bt_force_close(&s->bridge);
    return CP_BT_PROTOCOL_ERROR;
}
static int verified(struct cp_step2_ipc_link *s){
    if (!s || s->poisoned || s->finished || !s->connected ||
        !s->radio_generation || !s->owner.owner_alive ||
        s->owner.owner_alive(s->owner.opaque,s->peer,s->radio_generation)!=1)
        return 0;
    return 1;
}
static int transport_verify(void *v,uint32_t generation){
    struct cp_step2_ipc_link *s=(struct cp_step2_ipc_link *)v;
    return s && generation==s->local_generation && verified(s);
}
static int transport_submit(void *v,uint32_t generation,const uint8_t *p,size_t n){
    struct cp_step2_ipc_link *s=(struct cp_step2_ipc_link *)v;
    if (!s || !p || !n || n>CP_BT_PAYLOAD_MAX ||
        generation!=s->local_generation || !verified(s) ||
        !s->owner.emit_frame) return -1;
    s->pending_frame[0]=2; s->pending_frame[1]=0;
    s->pending_frame[2]=(uint8_t)n;s->pending_frame[3]=(uint8_t)(n>>8);
    put32(s->pending_frame+4,s->radio_generation);
    memcpy(s->pending_frame+8,p,n);
    /* emit MUST copy all of this frame; its acceptance is not an ACK. */
    return s->owner.emit_frame(s->owner.opaque,s->pending_frame,n+8);
}
void cp_step2_ipc_init(struct cp_step2_ipc_link *s,
                       const struct cp_step2_ipc_owner *o){
    if (!s) return;
    memset(s,0,sizeof(*s));
    cp_bt_bridge_init(&s->bridge);
    cp_step2_transport_init(&s->transport);
    if (o) s->owner=*o;
    s->expected=8;
}
void cp_step2_ipc_lost(struct cp_step2_ipc_link *s){
    if (s) (void)poison(s);
}
static int frame(struct cp_step2_ipc_link *s){
    const uint8_t *p=s->receiving;
    const unsigned type=p[0];
    const size_t size=(size_t)p[2]|((size_t)p[3]<<8);
    const uint32_t radio_gen=read32(p+4);
    int rc;
    if (!radio_gen || p[1] || s->used!=size+8) return poison(s);
    if (type==1) {
        struct cp_step2_owner t_owner;
        if (size!=6 || s->connected || s->finished || s->radio_generation ||
            !s->owner.approve_peer || !s->owner.owner_alive ||
            !s->owner.emit_frame ||
            s->owner.approve_peer(s->owner.opaque,p+8,radio_gen)!=1 ||
            s->owner.owner_alive(s->owner.opaque,p+8,radio_gen)!=1)
            return poison(s);
        if (cp_bt_bridge_event(&s->bridge,CP_BT_REQUEST,0,0)!=CP_BT_OK ||
            cp_bt_request_decided(&s->bridge,1)!=CP_BT_OK ||
            cp_bt_bridge_event(&s->bridge,CP_BT_CONNECTED,0,0)!=CP_BT_OK)
            return poison(s);
        s->radio_generation=radio_gen;
        s->local_generation=s->bridge.generation;
        memcpy(s->peer,p+8,6);
        s->connected=1;
        t_owner.opaque=s;
        t_owner.verify=transport_verify;
        t_owner.submit_async=transport_submit;
        if (cp_step2_transport_bind(&s->transport,&s->bridge,&t_owner,
                                    s->local_generation)!=CP_BT_OK)
            return poison(s);
        return CP_BT_OK;
    }
    if (!verified(s) || radio_gen!=s->radio_generation)
        return poison(s);
    switch (type) {
    case 2:
        if (!size) return poison(s);
        rc=cp_step2_transport_rx(&s->transport,s->local_generation,p+8,size);
        if (rc!=CP_BT_OK)return poison(s);
        s->rx_frames++;
        return CP_BT_OK;
    case 3:
        if (size) return poison(s);
        rc=cp_step2_transport_disconnected(&s->transport,s->local_generation);
        if (rc!=CP_BT_OK)return poison(s);
        s->connected=0; s->finished=1; s->clean_disconnect=1;
        cp_bt_force_close(&s->bridge);
        return CP_BT_OK;
    case 4:
        if (size)return poison(s);
        rc=cp_step2_transport_tx_done(&s->transport,s->local_generation,1);
        if (rc!=CP_BT_OK)return poison(s);
        s->tx_confirmed++;
        return CP_BT_OK;
    default:return poison(s);
    }
}
int cp_step2_ipc_feed(struct cp_step2_ipc_link *s,const uint8_t *p,size_t n){
    if (!s || (!p && n))return CP_BT_BAD_ARG;
    if (s->poisoned || s->finished)return CP_BT_CLOSED;
    while (n) {
        size_t amount=s->expected-s->used;
        if (amount>n) amount=n;
        memcpy(s->receiving+s->used,p,amount);
        s->used+=amount;p+=amount;n-=amount;
        if (s->used==8 && s->expected==8) {
            size_t plen=(size_t)s->receiving[2]|
                        ((size_t)s->receiving[3]<<8);
            unsigned type=s->receiving[0];
            if (s->receiving[1] || !read32(s->receiving+4) ||
                !((type==1 && plen==6) ||
                  (type==2 && plen>0 && plen<=CP_BT_PAYLOAD_MAX) ||
                  ((type==3 || type==4) && plen==0)))
                return poison(s);
            s->expected=8+plen;
        }
        if (s->used==s->expected) {
            int rc=frame(s);
            if (rc!=CP_BT_OK) return rc;
            s->used=0;s->expected=8;
        }
    }
    return CP_BT_OK;
}
int cp_step2_ipc_read(struct cp_step2_ipc_link *s,
                      uint8_t *p,size_t n,size_t *read_count){
    if (read_count) *read_count=0;
    if (!s || !p || !n || !read_count)return CP_BT_BAD_ARG;
    if (!verified(s)) { (void)poison(s);return CP_BT_CLOSED; }
    return cp_step2_transport_read(&s->transport,p,n,read_count);
}
int cp_step2_ipc_write(struct cp_step2_ipc_link *s,const uint8_t *p,size_t n){
    if (!s || !p || !n || n>CP_BT_PAYLOAD_MAX)return CP_BT_BAD_ARG;
    if (!verified(s)) { (void)poison(s);return CP_BT_CLOSED; }
    int rc=cp_step2_transport_write(&s->transport,p,n);
    if (rc==CP_BT_PROTOCOL_ERROR)(void)poison(s);
    return rc;
}
