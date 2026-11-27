/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_BT_TRACE_LATCH_H
#define MU1005_BT_TRACE_LATCH_H
#include <stdint.h>
/* Single process lifetime, caller-owned zero-initialized storage. Bind only
 * after exact mapped-image verification and independently resolving the
 * original trace target/channel. Never authorizes SDK ownership. */
typedef void (*MuBtTraceOriginal)(void*,int,const char*,...);
typedef struct {
    volatile unsigned bound, pending;
    uintptr_t channel;
    MuBtTraceOriginal original;
} MuBtTraceLatch;
int mu_bt_trace_latch_bind(MuBtTraceLatch*,uintptr_t,MuBtTraceOriginal);
MuBtTraceOriginal mu_bt_trace_latch_observe(MuBtTraceLatch*,uintptr_t,uintptr_t,uintptr_t);
int mu_bt_trace_latch_take(MuBtTraceLatch*);
#endif
