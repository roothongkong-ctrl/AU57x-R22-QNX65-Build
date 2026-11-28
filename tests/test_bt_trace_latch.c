/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_trace_latch.h"
#include <assert.h>
#include <stdio.h>
static void original(void *p,int n,const char *s,...){(void)p;(void)n;(void)s;}
static void other(void *p,int n,const char *s,...){(void)p;(void)n;(void)s;}
void mu_bt_trace_latch_test(void){
    MuBtTraceLatch s={0};
    assert(!mu_bt_trace_latch_take(&s));
    assert(!mu_bt_trace_latch_observe(&s,0x11ed14,0x1234,0x27cf94));
    assert(!mu_bt_trace_latch_bind(&s,0x1235,original));
    assert(!mu_bt_trace_latch_bind(&s,0x1234,0));
    assert(mu_bt_trace_latch_bind(&s,0x1234,original));
    assert(!mu_bt_trace_latch_bind(&s,0x4324,other));
    assert(mu_bt_trace_latch_observe(&s,0x11ed10,0x1234,0x27cf94)==original);
    assert(mu_bt_trace_latch_observe(&s,0x11ed14,0x4324,0x27cf94)==original);
    assert(mu_bt_trace_latch_observe(&s,0x11ed14,0x1234,0x27cf98)==original);
    assert(!mu_bt_trace_latch_take(&s));
    for(int i=0;i<3;i++)assert(mu_bt_trace_latch_observe(&s,0x11ed14,0x1234,0x27cf94)==original);
    assert(mu_bt_trace_latch_take(&s));assert(!mu_bt_trace_latch_take(&s));
    assert(mu_bt_trace_latch_observe(&s,0x11ed14,0x1234,0x27cf94)==original);
    assert(mu_bt_trace_latch_take(&s));
    MuBtTraceLatch d={0};
    assert(mu_bt_trace_latch_bind_deferred(&d,original));
    assert(!mu_bt_trace_latch_bind(&d,0x1234,other));
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed14,0,0x27cf94,0)==original);
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed14,0x1234,0x27cf94,0)==original);
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed14,0x1234,0x27cf94,0x5678)==original);
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed10,0x1234,0x27cf94,0x1234)==original);
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed14,0x1235,0x27cf94,0x1235)==original);
    assert(!mu_bt_trace_latch_take(&d));
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed14,0x1234,0x27cf94,0x1234)==original);
    assert(mu_bt_trace_latch_take(&d));
    assert(mu_bt_trace_latch_observe_live(&d,0x11ed14,0x5678,0x27cf94,0x5678)==original);
    assert(mu_bt_trace_latch_take(&d));
    assert(mu_bt_trace_latch_observe(&d,0x11ed14,0,0x27cf94)==original);
    assert(!mu_bt_trace_latch_take(&d));
    puts("MU1005_TRACE_DEFERRED_CHANNEL_PASS_FIXTURE_ONLY");
    puts("MU1005_TRACE_LATCH_COALESCING_PASS_FIXTURE_ONLY");
}
#ifdef TRACE_LATCH_STANDALONE
int main(void){mu_bt_trace_latch_test();return 0;}
#endif
