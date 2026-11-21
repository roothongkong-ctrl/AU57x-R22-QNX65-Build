/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_step2_transport.h"
#include <string.h>
void cp_step2_transport_init(struct cp_step2_transport *t){
    if(t) memset(t,0,sizeof(*t));
}
void cp_step2_transport_quarantine(struct cp_step2_transport *t){
    if(!t)return;
    if(t->bridge) cp_bt_force_close(t->bridge);
    t->bound=0;
    t->generation=0;
    t->stream.has_packet=0;
    t->stream.position=0;
    t->stream.packet.len=0;
}
static int allowed(struct cp_step2_transport *t){
    if(!t||!t->bridge||!t->bound||!t->generation)return 0;
    if(t->bridge->closed||!t->bridge->connected||
       t->bridge->generation!=t->generation)return 0;
    if(!t->owner.verify||t->owner.verify(t->owner.opaque,t->generation)!=1){
        cp_step2_transport_quarantine(t);
        return 0;
    }
    return 1;
}
int cp_step2_transport_bind(struct cp_step2_transport *t,
                            struct cp_bt_bridge *bridge,
                            const struct cp_step2_owner *owner,
                            uint32_t g){
    if(!t||!bridge||!owner||!owner->verify||!owner->submit_async||!g)
        return CP_BT_BAD_ARG;
    if(t->bound)return CP_BT_BUSY;
    if(bridge->closed||!bridge->connected||bridge->generation!=g)
        return CP_BT_NOT_CONNECTED;
    if(owner->verify(owner->opaque,g)!=1)return CP_BT_PROTOCOL_ERROR;
    cp_bt_stream_init(&t->stream,bridge);
    t->bridge=bridge;
    t->owner=*owner;
    t->generation=g;
    t->bound=1;
    return CP_BT_OK;
}
int cp_step2_transport_rx(struct cp_step2_transport *t,uint32_t g,
                          const uint8_t *data,size_t n){
    if(!t||!t->bound)return CP_BT_NOT_CONNECTED;
    /* Stale packets are dropped before any vendor-owner callbacks. */
    if(g!=t->generation)return CP_BT_STALE;
    if(!allowed(t))return CP_BT_CLOSED;
    int rc=cp_bt_bridge_event_tagged(t->bridge,g,CP_BT_DATA,data,n);
    if(rc==CP_BT_OVERFLOW||rc==CP_BT_PROTOCOL_ERROR)
        cp_step2_transport_quarantine(t);
    return rc;
}
int cp_step2_transport_read(struct cp_step2_transport *t,
                            uint8_t *dst,size_t n,size_t *read_count){
    if(read_count)*read_count=0;
    if(!t||!read_count)return CP_BT_BAD_ARG;
    if(!allowed(t))return CP_BT_CLOSED;
    int rc=cp_bt_stream_read(&t->stream,t->generation,dst,n,read_count);
    if(rc==CP_BT_PROTOCOL_ERROR||rc==CP_BT_OVERFLOW)
        cp_step2_transport_quarantine(t);
    return rc;
}
static int sdk_submit(void *p,uint32_t g,const uint8_t *d,size_t n){
    struct cp_step2_transport *t=(struct cp_step2_transport*)p;
    /* Caller must not synchronize callback recursively inside submit_async. */
    if(!t||!t->bound||!t->owner.submit_async||g!=t->generation)
        return -1;
    return t->owner.submit_async(t->owner.opaque,g,d,n);
}
int cp_step2_transport_write(struct cp_step2_transport *t,
                             const uint8_t *src,size_t n){
    if(!t)return CP_BT_BAD_ARG;
    if(!allowed(t))return CP_BT_CLOSED;
    int rc=cp_bt_stream_submit(&t->stream,t->generation,src,n,sdk_submit,t);
    if(rc==CP_BT_PROTOCOL_ERROR)cp_step2_transport_quarantine(t);
    return rc;
}
int cp_step2_transport_tx_done(struct cp_step2_transport *t,
                               uint32_t g,int sdk_ok){
    if(!t||!t->bound)return CP_BT_NOT_CONNECTED;
    if(g!=t->generation)return CP_BT_STALE;
    /* A failed/unknown SDK completion cannot be retried safely. */
    if(!sdk_ok){cp_step2_transport_quarantine(t);return CP_BT_PROTOCOL_ERROR;}
    if(!allowed(t))return CP_BT_CLOSED;
    int rc=cp_bt_bridge_event_tagged(t->bridge,g,CP_BT_TX_COMPLETE,0,0);
    if(rc==CP_BT_PROTOCOL_ERROR)cp_step2_transport_quarantine(t);
    return rc;
}
int cp_step2_transport_disconnected(struct cp_step2_transport *t,uint32_t g){
    if(!t||!t->bound)return CP_BT_NOT_CONNECTED;
    if(g!=t->generation)return CP_BT_STALE;
    int rc=cp_bt_bridge_event_tagged(t->bridge,g,CP_BT_DISCONNECTED,0,0);
    t->bound=0;
    t->generation=0;
    t->stream.has_packet=0;
    t->stream.packet.len=0;
    t->stream.position=0;
    return rc;
}
