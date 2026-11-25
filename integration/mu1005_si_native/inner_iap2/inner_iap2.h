/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU_INNER_IAP2_H
#define MU_INNER_IAP2_H
#include <stddef.h>
#include <stdint.h>
#define MU_INNER_CAP 65535u
/* Owner-thread only, one instance per actual AirPlay session. No auth bypass.
 * send_event: 1 accepted entire packet; 0 retry with no effects; -1 terminal.
 * write_cinemo: positive bytes committed; 0 backpressure; -1 terminal.
 * owner: 1 only for current OEM session and established event/control channels.
 * The adapter must serialize callbacks and never reenter this object. */
typedef struct {
 void *ctx;
 int (*owner)(void *,uint64_t);
 int (*send_event)(void *,const unsigned char *,size_t);
 int (*write_cinemo)(void *,const unsigned char *,size_t);
} MuInnerOps;
typedef struct {
 MuInnerOps ops;
 uint64_t epoch;
 size_t tx_len,rx_len,rx_off;
 int active;
 unsigned char tx[MU_INNER_CAP],rx[MU_INNER_CAP];
} MuInner;
int mu_inner_start(MuInner *,uint64_t,const MuInnerOps *);
void mu_inner_end(MuInner *);
/* 1 accepted all bytes, 0 retry same bytes, -1 terminal. */
int mu_inner_from_cinemo(MuInner *,uint64_t,const unsigned char *,size_t);
/* One complete iAPSendMessage packet from decrypted control channel.
 * 1 queued (ACK may be sent), 0 busy retry, -1 invalid/terminal. */
int mu_inner_from_control(MuInner *,uint64_t,const unsigned char *,size_t);
/* Nonblocking bounded tick. 1 live, -1 ended. */
int mu_inner_pump(MuInner *,uint64_t);
#endif
