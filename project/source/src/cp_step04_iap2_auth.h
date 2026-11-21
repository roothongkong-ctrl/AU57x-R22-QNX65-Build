/* SPDX-License-Identifier: GPL-3.0-only
 * iAP2 CONTROL SESSION + original-stock MFi challenge/response adapter.
 * No raw RFCOMM frame may be passed directly to this API.
 */
#ifndef CP_STEP04_IAP2_AUTH_H
#define CP_STEP04_IAP2_AUTH_H
#include "cp_step04_stock_mfi.h"
#include "cp_oem_boundary.h"
#include <stddef.h>
#include <stdint.h>
enum cp_auth_stage { CP_AUTH_IDLE=0, CP_AUTH_AWAIT_CERT=1,
 CP_AUTH_CERT_PENDING=2, CP_AUTH_AWAIT_CHALLENGE=3,
 CP_AUTH_SIGNATURE_PENDING=4, CP_AUTH_AWAIT_PEER=5,
 CP_AUTH_SUCCEEDED=6, CP_AUTH_FAILED=7 };
enum cp_auth_rc { CP_AUTH_OK=0, CP_AUTH_TX_PENDING=1,
 CP_AUTH_UNRELATED=2,
 CP_AUTH_BAD=-1, CP_AUTH_OWNER=-2, CP_AUTH_ORDER=-3,
 CP_AUTH_OEM=-4, CP_AUTH_BLOCKED=-5 };
struct cp_auth_owner {
 void *opaque;
 int (*control_live)(void *,uint32_t epoch,uint32_t bt_generation);
 int (*wired_idle)(void *);
 int (*send_csm)(void *,uint32_t epoch,uint32_t bt_generation,
                 const uint8_t *,size_t,uint32_t token);
 int (*tx_committed)(void *,uint32_t epoch,uint32_t bt_generation,
                     uint32_t token,uint16_t message_id);
 int (*peer_auth_succeeded)(void *,uint32_t epoch,uint32_t bt_generation);
};
struct cp_step04_auth {
 struct cp_stock_mfi *stock;
 struct cp_auth_owner owner;
 uint32_t epoch,bt_generation,sequence,tx_token;
 uint16_t tx_id;
 enum cp_auth_stage stage;
};
/* The owner must deliver complete CSM frames from a verified iAP2
 * CONTROL session 10 and serialize all calls on its original thread. */
int cp_auth_init(struct cp_step04_auth *,struct cp_stock_mfi *,
                 const struct cp_auth_owner *,uint32_t epoch,uint32_t bt_generation);
int cp_auth_receive(struct cp_step04_auth *,const uint8_t *,size_t);
int cp_auth_tx_confirm(struct cp_step04_auth *,uint32_t token);
int cp_auth_live(struct cp_step04_auth *);
int cp_auth_revoke(struct cp_step04_auth *);
/* Derive verified_mfi from live session; never trust caller's asserted flag. */
int cp_step04_dispatch(struct cp_step04_auth *,struct cp_wireless *,
    const struct cp_oem_assertions *other_proof,const uint8_t *,size_t);
#endif
