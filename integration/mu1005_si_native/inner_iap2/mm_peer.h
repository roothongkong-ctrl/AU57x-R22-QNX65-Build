/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU_INNER_MM_PEER_H
#define MU_INNER_MM_PEER_H
#include "inner_iap2.h"
/* One original mm-ipod device, owner serialized, zero initialized.
 * notify must post the original input-ready event, NOT transport_recv_pkt.
 * Only send/recv callbacks have the native transport slot signatures.
 * No module loader, transport identity selection or OEM notification hook. */
typedef struct {
 int fd, active, failed, ticking;
 void *device,*ctx;
 uint64_t epoch;
 int (*proof)(void *,void *,int,uint64_t);
 int (*notify)(void *,void *);
 size_t tx_used,rx_used;
 unsigned char tx[MU_INNER_CAP],rx[MU_INNER_CAP];
} MuMmPeer;
int mu_mm_peer_adopt(MuMmPeer *,void *,int,uint64_t,void *,
 int (*)(void *,void *,int,uint64_t),int (*)(void *,void *));
int mu_mm_peer_tick(MuMmPeer *);
int mu_mm_peer_send(void *,const void *,unsigned);
int mu_mm_peer_recv(void *,void *,unsigned,int,void *);
/* Requires original dispatch quiescence; fd is closed on revocation, but
 * storage stays owned until this explicit release. */
int mu_mm_peer_release(MuMmPeer *);
#endif
