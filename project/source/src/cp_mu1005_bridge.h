/* SPDX-License-Identifier: GPL-3.0-only
 * Pure owner-mediated callback snapshot -> normalized RFCOMM queue adapter.
 * No native BlueSDK hooks, iAP2 endpoint, MFi or firmware changes.
 */
#ifndef CP_MU1005_BRIDGE_H
#define CP_MU1005_BRIDGE_H
#include "cp_mu1005_callback.h"
#include "mu1005_rf_ingress.h"
#include <stdint.h>
enum cp_mu_bridge_result {
 CP_MB_OK=0,CP_MB_PENDING=1,
 CP_MB_BAD=-1,CP_MB_STALE=-2,CP_MB_BLOCKED=-3,CP_MB_OWNER=-4
};
struct cp_mu_bridge {
 AuRfIngress *rf;
 uint32_t epoch, wire_generation;
 uint8_t proposed_peer[6];
 int pending,accepted,connected,closed;
};
struct cp_mu_bridge_owner {
 struct cp_mu_reader snapshot;
 int (*tx_token_matches)(void *,uint32_t token,uint32_t wire_generation);
};
int cp_mu_bridge_init(struct cp_mu_bridge *b,AuRfIngress *rf,uint32_t epoch);
int cp_mu_bridge_request_decided(struct cp_mu_bridge *b,uint32_t epoch,
                                 int original_sdk_accept_confirmed);
int cp_mu_bridge_feed(struct cp_mu_bridge *b,const uint8_t snapshot[16],
                      uint32_t callback_epoch,const struct cp_mu_bridge_owner *owner,
                      uint8_t *owned_rx,size_t capacity);
#endif
