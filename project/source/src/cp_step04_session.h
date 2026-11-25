/* SPDX-License-Identifier: GPL-3.0-only
 * R44: one-authority integration for normalized RFCOMM, bounded iAP2 link,
 * and existing stock MFi CONTROL authentication. No native SDK hook.
 */
#ifndef CP_STEP04_SESSION_H
#define CP_STEP04_SESSION_H
#include "cp_step04_link.h"
#include "cp_step04_iap2_auth.h"
struct cp_s4_external {
 void *opaque;
 int (*bt_owner_live)(void *,uint32_t epoch,uint32_t generation);
 int (*control_owner_live)(void *,uint32_t epoch,uint32_t generation);
 int (*wired_idle)(void *);
 int (*peer_auth_succeeded)(void *,uint32_t epoch,uint32_t generation);
 int (*handoff_other_csm)(void *,const uint8_t *,size_t);
};
struct cp_step04_session {
 struct cp_step04_link link;
 struct cp_step04_auth auth;
 struct cp_stock_mfi *stock;
 struct cp_s4_external ext;
 struct cp_lk_owner link_owner;
 struct cp_auth_owner auth_owner;
 struct cp_wireless wireless;
 const struct cp_identification *identity;
 uint32_t identity_token;
 int identity_sent,identity_pending,identity_accepted;
 void *wireless_ctx;
 int (*wireless_proof)(void *,uint32_t,uint32_t,struct cp_oem_assertions *);
 uint32_t epoch,acked_token,wireless_seq,wireless_token;
 uint64_t now;
 int auth_ready, ack_in_progress,wireless_pending,wireless_bound;
 /* Coalesced, authenticated 5702 request awaiting live hotspot and TX slot. */
 int wifi_request_pending;
};
int cp_s4_init(struct cp_step04_session *,struct cp_stock_mfi *,
               uint32_t epoch,const struct cp_s4_external *);
int cp_s4_accept_rf(struct cp_step04_session *,const AuRfWire *,uint64_t now);
int cp_s4_flush_rf(struct cp_step04_session *,AuRfIngress *);
int cp_s4_tick(struct cp_step04_session *,uint64_t now);
int cp_s4_authenticated(struct cp_step04_session *);
/* Optional original-OEM identity over iAP2 CONTROL 1D00/1D01/1D02. */
int cp_s4_bind_identity(struct cp_step04_session *,
                        const struct cp_identification *);
/* Bind the live OEM radio + receiver proof provider and current AP identity;
 * caller owns config for entire single-generation link lifetime. */
int cp_s4_bind_wireless(struct cp_step04_session *,const struct cp_session *,
                        int (*)(void *,uint32_t,uint32_t,
                                 struct cp_oem_assertions *),void *);
/* Retry a previously received phone offer once OEM AirPlay is listening. */
int cp_s4_wireless_pump(struct cp_step04_session *);
/* Post-auth sender for cp_wireless_init(), with separately tracked ACK. */
int cp_s4_wireless_send(void *,const uint8_t *,size_t);
void cp_s4_revoke(struct cp_step04_session *);
#endif
