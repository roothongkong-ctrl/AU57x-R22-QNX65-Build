/* SPDX-License-Identifier: GPL-3.0-only
 * AU57x OEM Step 2: offline-only QNX iAP2 wire-transport ADAPTER CORE.
 * The actual BlueSDK owner, true iAP2 stack, MFi & authorization are NOT
 * implemented here. This code must not be installed or preloaded.
 */
#ifndef CP_STEP2_TRANSPORT_H
#define CP_STEP2_TRANSPORT_H
#include "cp_bt_stream.h"
#include <stddef.h>
#include <stdint.h>
/* All callbacks supplied by a separately audited IN-PROCESS QNX owner.
 * The caller serializes callbacks and bridge access, protects SDK-owned TX
 * buffers, and MUST close real BlueSDK channel if any API returns CLOSED.
 * verify() MUST check original process, paired peer, active physical RFCOMM,
 * event generation, and no concurrent OEM wired CarPlay owner.
 */
struct cp_step2_owner {
    void *opaque;
    int (*verify)(void *opaque, uint32_t physical_generation);
    int (*submit_async)(void *opaque, uint32_t physical_generation,
                        const uint8_t *bytes, size_t len);
};
struct cp_step2_transport {
    struct cp_bt_bridge *bridge;
    struct cp_bt_stream stream;
    struct cp_step2_owner owner;
    uint32_t generation;
    int bound;
};
void cp_step2_transport_init(struct cp_step2_transport *t);
/* bind ONLY after actual OEM has completed RFCOMM connection, and assigned
 * generation. Returns CP_BT_NOT_CONNECTED or CP_BT_PROTOCOL_ERROR if invalid.
 * Does not publish SDP, accept a Bluetooth connection, start OEM iAP2, or
 * falsely report MFi/CarPlay authentication.
 */
int cp_step2_transport_bind(struct cp_step2_transport *t,
                            struct cp_bt_bridge *bridge,
                            const struct cp_step2_owner *owner,
                            uint32_t physical_generation);
/* forward actual SDK event 5 (data); must not infer event generation at time
 * of deferred dispatch. No iAP2 decoding and no data fabrication. */
int cp_step2_transport_rx(struct cp_step2_transport *t,
                          uint32_t event_generation,
                          const uint8_t *bytes, size_t len);
/* byte-stream operations offered to an IAP2 link parser (NOT OEM callback ABI).
 * read() returns CP_BT_EMPTY when no data; both operations are nonblocking. */
int cp_step2_transport_read(struct cp_step2_transport *t,
                            uint8_t *dst, size_t capacity, size_t *nread);
int cp_step2_transport_write(struct cp_step2_transport *t,
                             const uint8_t *src, size_t len);
/* SDK ACK event 6 and real link closure event 4, both generation-tagged.
 * sdk_ok=0 quarantines transport, never retransmits ambiguous bytes. */
int cp_step2_transport_tx_done(struct cp_step2_transport *t,
                               uint32_t event_generation, int sdk_ok);
int cp_step2_transport_disconnected(struct cp_step2_transport *t,
                                    uint32_t event_generation);
/* Quarantine is sticky until an independent, externally confirmed physical
 * disconnect and then explicit cp_bt_bridge_rearm(). */
void cp_step2_transport_quarantine(struct cp_step2_transport *t);
#endif
