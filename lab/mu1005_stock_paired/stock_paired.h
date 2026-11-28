/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_STOCK_PAIRED_H
#define MU1005_STOCK_PAIRED_H
#include <stddef.h>
#include <stdint.h>
/* Plan A: use the original paired list, no separate phone database.
 * Embed this as the first member of the existing serialized owner context.
 * The native selected callback must return the original, unique, currently
 * connected phone and its ACL generation, not a guessed USB identity.
 * certify must prove original image and owner serialization/lifetime. */
typedef struct {
 void *ctx;
 int (*certify)(void*);
 int (*selected)(void*,uint32_t*,uint8_t[6]);
 int (*read_live)(void*,uint32_t,void*,size_t);
} MuStockPaired;
int mu_stock_paired_snapshot(void*,uint32_t*);
#endif
