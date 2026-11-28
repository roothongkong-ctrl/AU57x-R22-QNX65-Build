/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_BT_TRACE_LATCH_H
#define MU1005_BT_TRACE_LATCH_H
#include <stdint.h>
/* Single process lifetime, caller-owned zero-initialized storage. Bind only
 * after exact mapped-image verification and independently resolving the
 * original trace target/channel. Never authorizes SDK ownership. */
typedef void (*MuBtTraceOriginal)(void*,int,const char*,...);
typedef struct {
    volatile unsigned bound, pending, acl_pending;
    uintptr_t channel;
    MuBtTraceOriginal original;
} MuBtTraceLatch;
int mu_bt_trace_latch_bind(MuBtTraceLatch*,uintptr_t,MuBtTraceOriginal);
MuBtTraceOriginal mu_bt_trace_latch_observe(MuBtTraceLatch*,uintptr_t,uintptr_t,uintptr_t);
/* Separate deferred mode: current is the live pinned stock channel slot
 * at the exact original call, not an externally supplied identity. */
int mu_bt_trace_latch_bind_deferred(MuBtTraceLatch*,MuBtTraceOriginal);
MuBtTraceOriginal mu_bt_trace_latch_observe_live(MuBtTraceLatch*,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
int mu_bt_trace_latch_take(MuBtTraceLatch*);
/* ACL trace is a coalesced notification to query the existing paired peer,
 * never a peer identity or permission to register a channel. */
#define MU_BT_ACL_UP 1u
#define MU_BT_ACL_DOWN 2u
void mu_bt_trace_acl_observe(MuBtTraceLatch*,uintptr_t lr,uintptr_t channel,uintptr_t format,uintptr_t current);
unsigned mu_bt_trace_acl_take(MuBtTraceLatch*);
unsigned mu_bt_trace_attached_acl_take(void);
#endif
