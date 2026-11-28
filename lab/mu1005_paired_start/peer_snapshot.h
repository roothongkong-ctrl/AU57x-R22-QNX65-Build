/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_PEER_SNAPSHOT_H
#define MU1005_PEER_SNAPSHOT_H
#include <stdint.h>
#include <stddef.h>
/* Search already active original RFCOMM registry entries for an independently
 * selected phone. Caller holds verified image and original owner lifetime.
 * 1 = unique stable association with existing bond, 0 = absent, -1 = unknown
 * or ambiguous. Neither bond nor registry membership is current ACL/security
 * authorization; do not use this alone to open a channel. No key reads. */
int mu1005_find_peer(const uint8_t selected[6],int(*read_live)(void*,uint32_t,void*,size_t),void*,uint32_t*);
#endif
