/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_CMGR_PEER_H
#define MU1005_CMGR_PEER_H
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
/* Borrowed original CMGR handler from the serialized OEM caller.
 * certify must independently establish exact image, original handler
 * lifetime, correct caller thread and that the original lock is NOT held.
 * The vendor getter acquires/releases its own lock. No new pair or key read.
 * This getter establishes device identity, not live ACL authorization. */
typedef struct {
 void *ctx;
 int (*certify)(void*,uint32_t handler);
 int (*read_live)(void*,uint32_t,void*,size_t);
} MuCmgrOwner;
int mu1005_cmgr_contract(const MuCmgrOwner*,uint32_t handler);
int mu1005_cmgr_remote(const MuCmgrOwner*,uint32_t handler,const uint8_t selected[6],uint32_t *remote);
#endif
