/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_trace_latch.h"
/* No SDK calls, reads through pointers, allocations, locks, I/O or logging
 * in this observer. The pending bit is a coalesced hint, NOT startup proof. */
int mu_bt_trace_latch_bind(MuBtTraceLatch *s,uintptr_t channel,MuBtTraceOriginal original){
    if(!s||!channel||(channel&3u)||!original)return 0;
    if(!__sync_bool_compare_and_swap(&s->bound,0u,1u))return 0;
    s->channel=channel;
    s->original=original;
    __sync_synchronize();
    (void)__sync_lock_test_and_set(&s->bound,2u);
    return 1;
}
MuBtTraceOriginal mu_bt_trace_latch_observe(MuBtTraceLatch *s,uintptr_t lr,uintptr_t channel,uintptr_t format){
    if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u)return 0;
    if(lr==0x11ed14u&&channel==s->channel&&format==0x27cf94u)
        (void)__sync_lock_test_and_set(&s->pending,1u);
    /* All valid observations forward the ORIGINAL, even on mismatch. */
    return s->original;
}
int mu_bt_trace_latch_take(MuBtTraceLatch *s){
    if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u)return 0;
    return __sync_bool_compare_and_swap(&s->pending,1u,0u);
}
