/* GPL-3.0-only; bounded, deterministic RFCOMM queue extracted from proven
 * design patterns in luka-dev's MHI2Q graft; no binary addresses are reused. */
#include "cp_bt_bridge.h"
#include <string.h>
static void reset_session(struct cp_bt_bridge *b) {
    b->rx_head=0; b->rx_count=0; b->pending_request=0;
    b->tx_pending=0; b->tx.len=0; b->tx.generation=0;
}
void cp_bt_bridge_init(struct cp_bt_bridge *b) {
    if(b) memset(b,0,sizeof(*b));
}
void cp_bt_force_close(struct cp_bt_bridge *b) {
    if(!b) return;
    b->connected=0; b->closed=1;
    reset_session(b);
}
int cp_bt_bridge_event(struct cp_bt_bridge *b,enum cp_bt_event_type e,
                       const uint8_t *data,size_t size) {
    if (!b || size>CP_BT_PAYLOAD_MAX || (size && !data)) return CP_BT_BAD_ARG;
    switch(e) {
    case CP_BT_REQUEST:
        if (size || b->closed || b->connected || b->pending_request) return CP_BT_PROTOCOL_ERROR;
        b->pending_request=1;
        return CP_BT_OK;
    case CP_BT_CONNECTED:
        if(size || b->closed || !b->pending_request) return CP_BT_PROTOCOL_ERROR;
        /* Monotonically identifies an RFCOMM connection; never reuse old packets. */
        b->generation++;
        if(!b->generation) b->generation=1;
        reset_session(b); b->connected=1;
        return CP_BT_OK;
    case CP_BT_DISCONNECTED:
        /* Untagged close is only meaningful before any active connection. */
        if(size) return CP_BT_BAD_ARG;
        if(b->connected) return CP_BT_PROTOCOL_ERROR;
        b->pending_request=0;
        return CP_BT_OK;
    case CP_BT_DATA:
    case CP_BT_TX_COMPLETE:
        /* Async event source may be an old connection: require generation. */
        return CP_BT_PROTOCOL_ERROR;
    default:
        return CP_BT_BAD_ARG;
    }
}
int cp_bt_bridge_event_tagged(struct cp_bt_bridge *b,uint32_t g,
                              enum cp_bt_event_type e,
                              const uint8_t *data,size_t size) {
    struct cp_bt_packet *p;
    unsigned index;
    if(!b || size>CP_BT_PAYLOAD_MAX || (size && !data) || !g)
        return CP_BT_BAD_ARG;
    if(e!=CP_BT_DISCONNECTED && e!=CP_BT_DATA && e!=CP_BT_TX_COMPLETE)
        return CP_BT_BAD_ARG;
    if(g!=b->generation) return CP_BT_STALE;
    switch(e) {
    case CP_BT_DISCONNECTED:
        if(size) return CP_BT_BAD_ARG;
        b->connected=0;
        reset_session(b);
        return CP_BT_OK;
    case CP_BT_DATA:
        if(!b->connected || b->closed) return CP_BT_NOT_CONNECTED;
        if(size==0) return CP_BT_BAD_ARG;
        if(b->rx_count>=CP_BT_QUEUE_CAP) {
            b->drops++;
            cp_bt_force_close(b); /* fail closed; no silent iAP2-byte loss */
            return CP_BT_OVERFLOW;
        }
        index=(b->rx_head+b->rx_count)%CP_BT_QUEUE_CAP;
        p=&b->rx[index];
        p->generation=b->generation;
        p->len=(uint16_t)size;
        memcpy(p->data,data,size);
        b->rx_count++;
        return CP_BT_OK;
    case CP_BT_TX_COMPLETE:
        if (size) return CP_BT_BAD_ARG;
        if(!b->connected || b->closed || !b->tx_pending) return CP_BT_NOT_CONNECTED;
        if(b->tx.generation!=b->generation) return CP_BT_PROTOCOL_ERROR;
        b->tx_pending=0;
        b->tx.len=0;
        return CP_BT_OK;
    default:
        return CP_BT_BAD_ARG;
    }
}
int cp_bt_request_decided(struct cp_bt_bridge *b,int accepted) {
    if(!b || !b->pending_request || b->connected || b->closed) return CP_BT_BAD_ARG;
    if(!accepted) b->pending_request=0;
    return CP_BT_OK;
}
int cp_bt_take_rx(struct cp_bt_bridge *b,struct cp_bt_packet *out) {
    if(!b || !out) return CP_BT_BAD_ARG;
    if(!b->rx_count) return CP_BT_EMPTY;
    if(!b->connected || b->closed) return CP_BT_NOT_CONNECTED;
    *out=b->rx[b->rx_head];
    b->rx_head=(b->rx_head+1)%CP_BT_QUEUE_CAP;
    b->rx_count--;
    b->delivered++;
    return CP_BT_OK;
}
int cp_bt_stage_tx(struct cp_bt_bridge *b,const uint8_t *data,size_t size) {
    if(!b || !data || size==0 || size>CP_BT_PAYLOAD_MAX) return CP_BT_BAD_ARG;
    if(b->closed || !b->connected) return CP_BT_NOT_CONNECTED;
    if(b->tx_pending) return CP_BT_BUSY;
    b->tx.generation=b->generation;
    b->tx.len=(uint16_t)size;
    memcpy(b->tx.data,data,size);
    b->tx_pending=1;
    return CP_BT_OK;
}
const struct cp_bt_packet *cp_bt_view_tx(const struct cp_bt_bridge *b) {
    return b && b->connected && !b->closed && b->tx_pending &&
           b->tx.generation==b->generation ? &b->tx : 0;
}

/* Recovery can only be authorized after the owning SDK adapter confirms
 * physical closure; this never calls BlueSDK or changes vehicle state.
 */
int cp_bt_bridge_rearm(struct cp_bt_bridge *b,int confirmed) {
    if(!b || !confirmed || !b->closed || b->connected) return CP_BT_BAD_ARG;
    reset_session(b);
    b->closed=0;
    return CP_BT_OK;
}
