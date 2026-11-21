/* SPDX-License-Identifier: GPL-3.0-only
 * Pure MU1005 16-byte BlueSDK callback SNAPSHOT parser.
 * No hook, SDK registration, arbitrary pointer dereference or MFi bypass.
 */
#ifndef CP_MU1005_CALLBACK_H
#define CP_MU1005_CALLBACK_H
#include <stdint.h>
#include <stddef.h>
#define CP_MU_MAX_PAYLOAD 5035u
enum cp_mu_result {
 CP_MU_OK=0, CP_MU_BAD=-1, CP_MU_STALE=-2,
 CP_MU_UNVERIFIED=-3, CP_MU_OVERFLOW=-4
};
struct cp_mu_callback {
 unsigned code;
 unsigned status;
 unsigned length;
 uint32_t token;
 uint32_t epoch;
 uint8_t peer_mac[6];
};
struct cp_mu_reader {
 void *opaque;
 /* Strict independent owner and paired identity attestations are mandatory. */
 int (*owner_thread)(void *);
 int (*paired_identity)(void *,uint32_t association,uint8_t mac[6]);
 /* Read only from verified live vendor memory; never use direct casts. */
 int (*read_live)(void *,uint32_t address,uint8_t *out,size_t n);
};
int cp_mu_decode_snapshot(const uint8_t *snapshot,size_t n,
      uint32_t expected_epoch,uint32_t callback_epoch,
      const struct cp_mu_reader *reader,
      struct cp_mu_callback *out,uint8_t *owned_rx,size_t rx_capacity);
#endif
