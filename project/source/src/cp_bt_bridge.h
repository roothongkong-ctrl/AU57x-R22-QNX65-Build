/* GPL-3.0-only. AU57x R3: protocol-neutral Bluetooth RFCOMM bridge core.
 * NOT a vendor BlueSDK driver or a link-layer iAP2 implementation.
 * The caller must serialize all calls under its OWN lock, outside SDK callbacks.
 */
#ifndef CP_BT_BRIDGE_H
#define CP_BT_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#define CP_BT_PAYLOAD_MAX 5035u
#define CP_BT_QUEUE_CAP 8u

enum cp_bt_event_type {
    CP_BT_REQUEST=1, CP_BT_CONNECTED=2, CP_BT_DISCONNECTED=4,
    CP_BT_DATA=5, CP_BT_TX_COMPLETE=6
};
enum cp_bt_result {
    CP_BT_OK=0, CP_BT_EMPTY=1, CP_BT_NOT_CONNECTED=2,
    CP_BT_BUSY=3, CP_BT_CLOSED=4, CP_BT_STALE=5, CP_BT_BAD_ARG=-1,
    CP_BT_OVERFLOW=-2, CP_BT_PROTOCOL_ERROR=-3
};
struct cp_bt_packet {
    uint32_t generation;
    uint16_t len;
    uint8_t data[CP_BT_PAYLOAD_MAX];
};
struct cp_bt_bridge {
    /* Connection events from actual Blue SDK require target-specific adapter. */
    uint32_t generation;
    uint32_t drops;
    uint32_t delivered;
    int connected;
    int closed;
    int pending_request;
    unsigned rx_head;
    unsigned rx_count;
    struct cp_bt_packet rx[CP_BT_QUEUE_CAP];
    struct cp_bt_packet tx;
    int tx_pending; /* Must retain bytes until event 6, never on send() return. */
};
void cp_bt_bridge_init(struct cp_bt_bridge *bridge);
/* Event 1 requests the external adapter to call the VERIFIED BlueSDK ACCEPT API;
 * this function only records the request. Event 5 copies SDK-owned data. */
int cp_bt_bridge_event(struct cp_bt_bridge *b,enum cp_bt_event_type event,
                       const uint8_t *bytes,size_t length);
/* Authoritative adapter reports that an inbound connection request was accepted. */
int cp_bt_request_decided(struct cp_bt_bridge *b,int accepted);
/* View belongs to caller, copied out; bytes are RFCOMM wire bytes, NOT CSM. */
int cp_bt_take_rx(struct cp_bt_bridge *b,struct cp_bt_packet *out);
/* One transmit packet at a time. External BlueSDK adapter must submit this
 * buffer and call TX_COMPLETE only for the same connection generation. */
int cp_bt_stage_tx(struct cp_bt_bridge *b,const uint8_t *bytes,size_t length);
const struct cp_bt_packet *cp_bt_view_tx(const struct cp_bt_bridge *b);
/* Connection-generation-aware handling of DISCONNECTED, DATA and TX_COMPLETE.
 * Generation is read from cp_bt_bridge.generation when the physical BlueSDK
 * connection is accepted, and carried by its adapter for each callback.
 * Must NOT infer it from the current state at callback delivery time.
 */
int cp_bt_bridge_event_tagged(struct cp_bt_bridge *b, uint32_t event_generation,
                              enum cp_bt_event_type event,
                              const uint8_t *bytes, size_t length);
/* After an overflow/quarantine the external owner MUST close the real BlueSDK
 * channel and receive confirmation before rearming. Re-init is NOT required.
 * A supplied true confirmation asserts real adapter state, not a host test.
 */
int cp_bt_bridge_rearm(struct cp_bt_bridge *b, int physical_disconnect_confirmed);
void cp_bt_force_close(struct cp_bt_bridge *b);
#endif

