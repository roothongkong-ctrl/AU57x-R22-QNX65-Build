/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_trace_latch.h"
/* No SDK calls, reads through pointers, allocations, locks, I/O or logging
 * in this observer. The pending bit is a coalesced hint, NOT startup proof. */
static int bind(MuBtTraceLatch *s,uintptr_t channel,MuBtTraceOriginal original){
    if(!s||(channel&3u)||!original)return 0;
    if(!__sync_bool_compare_and_swap(&s->bound,0u,1u))return 0;
    s->channel=channel;
    s->original=original;
    __sync_synchronize();
    (void)__sync_lock_test_and_set(&s->bound,2u);
    return 1;
}
int mu_bt_trace_latch_bind(MuBtTraceLatch *s,uintptr_t channel,MuBtTraceOriginal original){
    return channel?bind(s,channel,original):0;
}
/* Before original C++ channel construction, only the callsite and the
 * callable original are pinned. No channel pointer is guessed or retained. */
int mu_bt_trace_latch_bind_deferred(MuBtTraceLatch *s,MuBtTraceOriginal original){
    return bind(s,0,original);
}
MuBtTraceOriginal mu_bt_trace_latch_observe_live(MuBtTraceLatch *s,uintptr_t lr,uintptr_t channel,uintptr_t format,uintptr_t current){
    if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u)return 0;
    if(!s->channel&&lr==0x11ed14u&&format==0x27cf94u&&
       current&&!(current&3u)&&channel==current)
        (void)__sync_lock_test_and_set(&s->pending,1u);
    return s->original;
}
MuBtTraceOriginal mu_bt_trace_latch_observe(MuBtTraceLatch *s,uintptr_t lr,uintptr_t channel,uintptr_t format){
    if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u)return 0;
    if(s->channel&&lr==0x11ed14u&&channel==s->channel&&format==0x27cf94u)
        (void)__sync_lock_test_and_set(&s->pending,1u);
    /* All valid observations forward the ORIGINAL, even on mismatch. */
    return s->original;
}
int mu_bt_trace_latch_take(MuBtTraceLatch *s){
    if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u)return 0;
    return __sync_bool_compare_and_swap(&s->pending,1u,0u);
}

/* Exact stock callsite and live original channel, no vararg access.
 * Both flags can occur before consumption; worker must re-query live state. */
void mu_bt_trace_acl_observe(MuBtTraceLatch*s,uintptr_t lr,uintptr_t channel,uintptr_t format,uintptr_t current){
 if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u||
    s->channel||!current||(current&3u)||channel!=current)return;
 unsigned hint=0;
 if((lr==0x13f6c8u&&format==0x285d44u)||(lr==0x13f1dcu&&format==0x285bb0u))hint=MU_BT_ACL_UP;
 else if(lr==0x13f560u&&format==0x285d88u)hint=MU_BT_ACL_DOWN;
 if(hint)(void)__sync_fetch_and_or(&s->acl_pending,hint);
}
unsigned mu_bt_trace_acl_take(MuBtTraceLatch*s){
 if(!s||__sync_val_compare_and_swap(&s->bound,2u,2u)!=2u)return 0;
 return __sync_lock_test_and_set(&s->acl_pending,0u);
}
