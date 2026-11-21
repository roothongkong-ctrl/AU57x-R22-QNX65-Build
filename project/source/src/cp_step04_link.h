/* SPDX-License-Identifier: GPL-3.0-only
 * R43: bounded C99 port of upstream luka-dev iap2-link sans-I/O engine
 * (marker, SYN/LSP, control session 10, ACK, retransmission).
 * Adapted for MU1005 normalized RFCOMM queue; no OEM hooks/keys.
 */
#ifndef CP_STEP04_LINK_H
#define CP_STEP04_LINK_H
#include <stddef.h>
#include <stdint.h>
#include "mu1005_rf_ingress.h"
#define CP_LK_FRAME_MAX 1440u
#define CP_LK_RX_MAX 3072u
#define CP_LK_OUTPUT_SLOTS 8u
#define CP_LK_CONTROL_ID 10u

enum cp_lk_phase { CP_LK_IDLE=0, CP_LK_DETECT=1,
 CP_LK_NEGOTIATE=2, CP_LK_NORMAL=3, CP_LK_DEAD=4 };
enum cp_lk_result { CP_LK_OK=0, CP_LK_PENDING=1,
 CP_LK_BAD=-1, CP_LK_OWNER=-2, CP_LK_STALE=-3,
 CP_LK_BACKPRESSURE=-4, CP_LK_DEAD_LINK=-5 };
struct cp_lk_owner {
 void *opaque;
 int (*live)(void *,uint32_t epoch,uint32_t generation);
 int (*control)(void *,const uint8_t *,size_t);
 int (*acked)(void *,uint32_t app_token);
};
struct cp_lk_wire { size_t size;uint8_t data[CP_LK_FRAME_MAX]; };
struct cp_step04_link {
 struct cp_lk_owner owner;
 uint32_t epoch,generation;
 enum cp_lk_phase phase;
 uint8_t peer_mac[6];
 uint8_t have_peer_syn,peer_control,rx_psn,tx_psn;
 uint8_t outbound_pending,outbound_seq,retries,negotiate_tries;
 uint32_t outbound_token;
 size_t outbound_len;
 uint8_t outbound_data[CP_LK_FRAME_MAX-10u];
 uint64_t deadline;
 uint8_t rx[CP_LK_RX_MAX];size_t rx_used;
 struct cp_lk_wire output[CP_LK_OUTPUT_SLOTS];
 unsigned head,tail,count;
};
void cp_lk_init(struct cp_step04_link *);
int cp_lk_accept_rf(struct cp_step04_link *,const AuRfWire *,uint32_t epoch,
                    const struct cp_lk_owner *,uint64_t now_ms);
int cp_lk_feed_bytes(struct cp_step04_link *,const uint8_t *,size_t,uint64_t now_ms);
int cp_lk_queue_control(struct cp_step04_link *,const uint8_t *,size_t,
                        uint32_t app_token,uint64_t now_ms);
int cp_lk_tick(struct cp_step04_link *,uint64_t now_ms);
int cp_lk_drain_rf(struct cp_step04_link *,AuRfIngress *);
int cp_lk_peek(const struct cp_step04_link *,const uint8_t **,size_t *);
void cp_lk_revoke(struct cp_step04_link *);
#endif
