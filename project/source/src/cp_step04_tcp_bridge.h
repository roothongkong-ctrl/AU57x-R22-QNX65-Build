/* SPDX-License-Identifier: GPL-3.0-only
 * RFCOMM R14 TCP normalized framing <-> R44 iAP2/MFi link adapter.
 * Frame parser is independent of sockets; calling worker must verify peer PID,
 * origin, current generation, and serialize socket I/O. No car/OEM access.
 */
#ifndef CP_STEP04_TCP_BRIDGE_H
#define CP_STEP04_TCP_BRIDGE_H
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include "cp_step04_session.h"
struct cp_s4_tcp_bridge {
 struct cp_step04_session *session;
 void *opaque;
 int (*peer_authorized)(void *,uint32_t epoch);
 /* Optional normalized-frame gate, installed before the first CONNECT by
  * the R45 owner. Context must outlive the attached bridge. */
 void *rf_guard_ctx;
 int (*rf_guard)(void *,uint32_t epoch,uint32_t generation,const uint8_t mac[6]);
 uint32_t epoch,generation;
 uint8_t rx[AU_RF_WIRE_MAX];
 size_t rx_used,rx_expected;
 uint8_t tx[8u+CP_LK_FRAME_MAX];
 size_t tx_size,tx_sent;
 int connected,closed;
};
int cp_s4_tcp_init(struct cp_s4_tcp_bridge *, struct cp_step04_session *,
                   int (*peer_authorized)(void *,uint32_t),void *opaque);
/* Feed arbitrary socket read fragments. Every complete frame is handed to
 * the same R44 authenticated-generation/owner state machine. */
int cp_s4_tcp_feed(struct cp_s4_tcp_bridge *,const uint8_t *,size_t,uint64_t now);
/* Zero-copy read of a stable outgoing normalized frame; valid until commit. */
int cp_s4_tcp_peek(struct cp_s4_tcp_bridge *,const uint8_t **,size_t *);
int cp_s4_tcp_commit(struct cp_s4_tcp_bridge *,size_t bytes_sent);
int cp_s4_tcp_tick(struct cp_s4_tcp_bridge *,uint64_t now);
void cp_s4_tcp_eof(struct cp_s4_tcp_bridge *);
#endif
