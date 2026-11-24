/* SPDX-License-Identifier: GPL-3.0-only
 * MU1005 R45 -> upstream wireless daemon transport, no installation.
 * All calls serialized on the existing RFCOMM worker. No native owner discovery
 * or MFi implementation here. The owner and daemon attesters are mandatory.
 */
#ifndef AU57X_DAEMON_CLIENT_H
#define AU57X_DAEMON_CLIENT_H
#include "mu1005_socket_pump.h"
typedef struct {
    void *ctx;
    /* Current original owner + paired ACL + published service + no wired USB. */
    int (*admit)(void *,uint32_t,const uint8_t[6]);
    /* Independent confirmation of the expected local daemon and socket. */
    int (*daemon_verified)(void *,int);
} AuDaemonAdmission;
typedef struct {
    AuIpcStream stream;
    AuRfIngress *rf;
    AuDaemonAdmission guard;
    uint32_t generation;
    uint8_t peer[6];
    uint64_t started_ms;
    int fd,connecting,active;
} AuDaemonClient;
/* init once on fresh storage; stop before reinitializing; rf remains borrowed. */
void au_daemon_client_init(AuDaemonClient *,AuRfIngress *,AuDaemonAdmission);
int au_daemon_client_start(AuDaemonClient *,uint64_t);
int au_daemon_client_step(AuDaemonClient *,uint64_t);
int au_daemon_client_complete(AuDaemonClient *,int);
void au_daemon_client_stop(AuDaemonClient *);
#endif
