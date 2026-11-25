/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU_MM_NOTIFY_H
#define MU_MM_NOTIFY_H
#include <stdint.h>
/* Original mm-ipod owner must verify the exact loaded driver, current
 * initialized device/driver/transport, mapped extents, dispatch thread
 * and epoch. It must keep all objects alive during the callback.
 * No object discovery or OEM attachment is performed by this module. */
typedef int (*MuMmDriverRead)(void *);
typedef int (*MuMmNotifyProof)(void *,void *,void *,void *,uint64_t);
typedef struct {
 void *ctx,*handle,*device,*driver,*transport;
 uint64_t epoch;
 MuMmNotifyProof owner;
 MuMmDriverRead read;
 int bound,calling;
} MuMmNotify;
int mu_mm_notify_bind(MuMmNotify *,void *,void *,void *,void *,uint64_t,void *,MuMmNotifyProof);
int mu_mm_notify_input(void *,void *);
void mu_mm_notify_clear(MuMmNotify *);
#endif
