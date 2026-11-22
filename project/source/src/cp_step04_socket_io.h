/* SPDX-License-Identifier: GPL-3.0-only
 * Nonblocking TCP worker for the original R14 normalized RFCOMM stream.
 * Review only: an independently attested OEM owner must supply the accepted
 * 127.0.0.1:57342 socket. TCP loopback alone is NOT proof of peer identity.
 */
#ifndef CP_STEP04_SOCKET_IO_H
#define CP_STEP04_SOCKET_IO_H
#include <stdint.h>
#include "cp_step04_tcp_bridge.h"
#define CP_S4_LOCAL_PORT 57342u
struct cp_s4_socket_io {
 struct cp_s4_tcp_bridge *bridge;
 void *opaque;
 int (*peer_verified)(void *,int fd,uint32_t epoch);
 int fd,attached;
};
/* Does NOT bind/listen/accept, authenticate iPhone, or install any OEM hook.
 * Caller owns fd until attach succeeds; then worker owns it until abort.
 * fd MUST be an accepted loopback connection on local TCP port 57342. */
int cp_s4_socket_attach(struct cp_s4_socket_io *,struct cp_s4_tcp_bridge *,
                        int fd,int (*peer_verified)(void *,int,uint32_t),
                        void *opaque);
/* Run from a serialized, authorized owner thread. Never from OEM callback. */
int cp_s4_socket_poll(struct cp_s4_socket_io *,uint64_t now_ms);
void cp_s4_socket_abort(struct cp_s4_socket_io *);
#endif
