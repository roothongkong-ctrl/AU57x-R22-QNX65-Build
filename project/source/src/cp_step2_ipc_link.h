/* SPDX-License-Identifier: GPL-3.0-only
 * AU57x R11 offline integration: R7 BlueSDK IPC wire <-> R9 iAP2 byte stream.
 * Raw Bluetooth bytes, NOT iAP2 authentication, SDK startup or a QNX plugin.
 * Exactly one trusted physical connection per instance; no automatic rearm.
 * All calls MUST be serialized by an independently verified owner thread.
 */
#ifndef CP_STEP2_IPC_LINK_H
#define CP_STEP2_IPC_LINK_H
#include "cp_step2_transport.h"
#include <stdint.h>
#include <stddef.h>
#define CP_STEP2_IPC_MAX (8u + CP_BT_PAYLOAD_MAX)
/* Callback return 1 means affirmative peer/owner verification; 0 or errors
 * reject. emit_frame() returns 0 ONLY if ALL bytes were copied into a
 * bounded outbound queue for the existing authenticated loopback IPC peer;
 * it is NOT a BlueSDK transmit-completion callback. Socket write != radio TX.
 */
struct cp_step2_ipc_owner {
    void *opaque;
    int (*approve_peer)(void *, const uint8_t mac[6], uint32_t radio_generation);
    int (*owner_alive)(void *, const uint8_t mac[6], uint32_t radio_generation);
    int (*emit_frame)(void *, const uint8_t *complete_frame, size_t frame_size);
};
struct cp_step2_ipc_link {
    struct cp_bt_bridge bridge;
    struct cp_step2_transport transport;
    struct cp_step2_ipc_owner owner;
    uint8_t pending_frame[CP_STEP2_IPC_MAX];
    uint8_t receiving[CP_STEP2_IPC_MAX];
    uint8_t peer[6];
    size_t used, expected;
    uint32_t radio_generation, local_generation;
    int connected, finished, poisoned, clean_disconnect;
    unsigned long rx_frames, tx_confirmed;
};
void cp_step2_ipc_init(struct cp_step2_ipc_link *, const struct cp_step2_ipc_owner *);
/* Arbitrary TCP fragmentation. Type 1 OPEN+MAC, 2 DATA, 3 DISCONNECT,
 * 4 SDK-completed ACK. Each framed message has 8-byte header and matching
 * radio generation. Any corruption fails CLOSED; never infer radio state. */
int cp_step2_ipc_feed(struct cp_step2_ipc_link *, const uint8_t *, size_t);
/* Reads exactly the bytes received from the actual RFCOMM data callbacks;
 * no iAP2 framing or credentials are generated here. */
int cp_step2_ipc_read(struct cp_step2_ipc_link *, uint8_t *, size_t, size_t *);
/* Emits type-2 data to the R7 bridge; ACK only on type-4 coming from the
 * final real BlueSDK event-6 (after every chunk has completed). */
int cp_step2_ipc_write(struct cp_step2_ipc_link *, const uint8_t *, size_t);
/* Never auto-reconnect or silently swallow unacknowledged sends. */
void cp_step2_ipc_lost(struct cp_step2_ipc_link *);
#endif
