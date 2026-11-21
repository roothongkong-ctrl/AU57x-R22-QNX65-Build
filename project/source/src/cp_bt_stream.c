/* GPL-3.0-only. Full-duplex stream mechanics; vendor transport glue absent. */
#include "cp_bt_stream.h"
#include <string.h>

void cp_bt_stream_init(struct cp_bt_stream *s, struct cp_bt_bridge *bridge) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->bridge = bridge;
}
static void discard_pending(struct cp_bt_stream *s) {
    s->has_packet = 0;
    s->position = 0;
    s->packet.len = 0;
}
int cp_bt_stream_read(struct cp_bt_stream *s, uint32_t expected_generation,
                      uint8_t *out, size_t capacity, size_t *received) {
    size_t total = 0;
    struct cp_bt_bridge *b;
    if (received) *received = 0;
    if (!s || !s->bridge || !received || !expected_generation ||
        !out || !capacity) return CP_BT_BAD_ARG;
    b = s->bridge;
    if (b->generation != expected_generation) return CP_BT_STALE;
    if (b->closed || !b->connected) {
        discard_pending(s);
        s->generation = expected_generation;
        return b->closed ? CP_BT_CLOSED : CP_BT_NOT_CONNECTED;
    }
    if (s->generation != expected_generation) {
        discard_pending(s);
        s->generation = expected_generation;
    }
    while (total < capacity) {
        size_t available, chunk;
        int r;
        if (!s->has_packet) {
            r = cp_bt_take_rx(b, &s->packet);
            if (r == CP_BT_EMPTY) break;
            if (r != CP_BT_OK) {
                discard_pending(s);
                return r;
            }
            if (s->packet.generation != expected_generation || !s->packet.len) {
                /* A mismatched packet is a broken adapter contract. */
                cp_bt_force_close(b);
                discard_pending(s);
                return CP_BT_PROTOCOL_ERROR;
            }
            s->position = 0;
            s->has_packet = 1;
        }
        available = (size_t)s->packet.len - s->position;
        chunk = capacity - total < available ? capacity - total : available;
        memcpy(out + total, s->packet.data + s->position, chunk);
        total += chunk;
        s->position += chunk;
        if (s->position == (size_t)s->packet.len) discard_pending(s);
    }
    *received = total;
    s->rx_bytes += total;
    return total ? CP_BT_OK : CP_BT_EMPTY;
}
int cp_bt_stream_submit(struct cp_bt_stream *s, uint32_t expected_generation,
                        const uint8_t *bytes, size_t length,
                        cp_bt_submit_frame submit, void *submit_ctx) {
    struct cp_bt_bridge *b;
    const struct cp_bt_packet *tx;
    int r;
    if (!s || !s->bridge || !expected_generation || !bytes || !length ||
        length > CP_BT_PAYLOAD_MAX || !submit) return CP_BT_BAD_ARG;
    b = s->bridge;
    if (b->generation != expected_generation) return CP_BT_STALE;
    if (b->closed || !b->connected) return CP_BT_NOT_CONNECTED;
    r = cp_bt_stage_tx(b, bytes, length);
    if (r != CP_BT_OK) return r;
    tx = cp_bt_view_tx(b);
    if (!tx || tx->generation != expected_generation || tx->len != length) {
        cp_bt_force_close(b);
        return CP_BT_PROTOCOL_ERROR;
    }
    if (submit(submit_ctx, expected_generation, tx->data, tx->len) != 0) {
        /* Submission could be partial; safest response is disconnect, no retry. */
        cp_bt_force_close(b);
        return CP_BT_PROTOCOL_ERROR;
    }
    s->tx_submitted += (uint64_t)length;
    return CP_BT_OK;
}
