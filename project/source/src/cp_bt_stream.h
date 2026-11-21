/* GPL-3.0-only. Byte-stream adapter for the AU57x candidate RFCOMM transport.
 * Host and generic ARM source ONLY: this is not an OEM BlueSDK binding.
 * Event source must own/serialize bridge and stream under the same lock.
 */
#ifndef CP_BT_STREAM_H
#define CP_BT_STREAM_H
#include "cp_bt_bridge.h"
#include <stddef.h>
#include <stdint.h>

/* The real OEM callback adapter must provide a send function that returns 0
 * ONLY when a whole payload has been accepted for ASYNC sending.  Completion
 * MUST be delivered separately via cp_bt_bridge_event_tagged(...TX_COMPLETE).
 * No SDK callbacks may synchronously reenter functions while owner lock held.
 */
typedef int (*cp_bt_submit_frame)(void *ctx, uint32_t generation,
                                  const uint8_t *data, size_t length);

struct cp_bt_stream {
    struct cp_bt_bridge *bridge; /* not owned */
    struct cp_bt_packet packet;
    size_t position;
    int has_packet;
    uint32_t generation;
    uint64_t rx_bytes;
    uint64_t tx_submitted;
};
void cp_bt_stream_init(struct cp_bt_stream *s, struct cp_bt_bridge *bridge);
/* read up to capacity bytes, including across SDK frames; never pad/drop.
 * expected_generation must be captured at the physical SDK callback boundary.
 * returned count is always initialized (zero on non-OK results).
 */
int cp_bt_stream_read(struct cp_bt_stream *s, uint32_t expected_generation,
                      uint8_t *out, size_t capacity, size_t *received);
/* Submit exactly one SDK frame and retain TX buffer until exact-generation
 * completion callback.  If submit outcome is ambiguous/failure: fail CLOSED;
 * never retransmit unknown partially delivered iAP2 bytes.
 */
int cp_bt_stream_submit(struct cp_bt_stream *s, uint32_t expected_generation,
                        const uint8_t *bytes, size_t length,
                        cp_bt_submit_frame submit, void *submit_ctx);
#endif
