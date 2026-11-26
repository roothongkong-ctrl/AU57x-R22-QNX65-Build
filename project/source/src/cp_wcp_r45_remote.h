/* SPDX-License-Identifier: GPL-3.0-only
 * R89: original MU1005 btstack -> verified AF_INET loopback -> R44 -> R87.
 * Unlike R88 same-process borrowed AuRfIngress, this receiver-side adapter
 * NEVER dereferences native btstack memory from the daemon's address space.
 * Compile/review only. Not a native hook or an OEM provenance provider.
 */
#ifndef CP_WCP_R45_REMOTE_H
#define CP_WCP_R45_REMOTE_H
#include <stdint.h>
#include "cp_wcp_autoconnect.h"
#include "cp_step04_socket_io.h"

struct cp_wcp_r45_remote_owner {
 void *ctx;
 /* Independent OEM evidence; neither socket data nor caller-provided MAC
  * proves the original process/paired iPhone identity on its own. */
 int (*owner_verified)(void *,uint32_t epoch,uint32_t generation);
 int (*server_published)(void *,uint32_t epoch,uint32_t generation);
 int (*paired_acl_authorized)(void *,uint32_t epoch,uint32_t generation,
                              const uint8_t mac[6]);
 int (*wired_idle)(void *);
 int (*transport_verified)(void *,int fd,uint32_t epoch,uint32_t generation);
};
struct cp_wcp_r45_remote {
 struct cp_s4_socket_io *worker;
 struct cp_s4_tcp_bridge *bridge;
 struct cp_wcp_r45_remote_owner o;
 uint32_t epoch,generation;
 uint8_t peer[6];
 int bound;
};
/* Bind ONLY after the validated R14 CONNECT frame has passed through R44's
 * TCP parser on an independently verified, accepted loopback socket.
 * This never creates/accepts a new OEM connection. */
int cp_wcp_r45_remote_bind(struct cp_wcp_r45_remote *,
 struct cp_s4_socket_io *,struct cp_s4_tcp_bridge *,
 const struct cp_wcp_r45_remote_owner *);
/* Returns R87's existing transport callback surface for a proven already-
 * connected, server-first original RFCOMM link. A transport CONNECT alone
 * is never iAP2 READY: wait for the validated R44 SYN/LSP/ACK exchange.
 * Still no invented client-open ABI or MFi bypass. */
int cp_wcp_r45_remote_ops(struct cp_wcp_r45_remote *,struct cp_wcp_bt_ops *);
/* Single owner tick for the initial server-first connection. Polls real
 * accepted TCP, binds R87 to the decoded CONNECT, advances SYN/LSP/ACK
 * and AA00-AA05 using the existing R44 session. No client-open guessing.
 * r and ac start zeroed; the caller must supply the independently verified
 * original OEM owner. WAIT during handshake, OK only after MFi succeeds.
 * After OK hand off to cp_wcp_begin_cloud and continue the same socket
 * owner; do not create a second reader or use this to manage BT retirement. */
int cp_wcp_r45_bootstrap_tick(struct cp_wcp_r45_remote *,
 struct cp_s4_socket_io *,struct cp_s4_tcp_bridge *,
 const struct cp_wcp_r45_remote_owner *,struct cp_wcp_autoconnect *,uint64_t);
/* Not safe to reuse an old session generation after disconnect/USB takeover. */
void cp_wcp_r45_remote_unbind(struct cp_wcp_r45_remote *);
#endif
