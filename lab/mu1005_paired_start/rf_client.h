/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_RF_CLIENT_H
#define MU1005_RF_CLIENT_H
#include "cmgr_peer.h"
/* Existing owner must attest the real SDP result, actual registered iAP2
 * client descriptor/callback, selected peer, credit profile and wired idle.
 * This adapter does not allocate vendor objects or synthesize authentication.
 * Objects remain owned by original caller until original cancellation. */
typedef struct {
 MuCmgrOwner cmgr;
 int (*client_ready)(void*,uint32_t handler,uint32_t remote,uint32_t descriptor,
                     unsigned scn,unsigned profile);
} MuRfClientOwner;
int mu1005_rf_client_contract(const MuRfClientOwner*,uint32_t,uint32_t,uint32_t,unsigned,unsigned);
int mu1005_rf_client_open(const MuRfClientOwner*,uint32_t,const uint8_t[6],uint32_t,unsigned,unsigned);
#endif
