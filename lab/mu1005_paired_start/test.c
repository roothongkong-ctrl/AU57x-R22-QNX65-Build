/* SPDX-License-Identifier: GPL-3.0-only */
#include "paired_start.h"
#include "bt_trace_latch.h"
#include <assert.h>
#include <stdio.h>
typedef struct {MuBtTraceLatch latch;uint32_t gen;int seen,wired,link,cancel,release,opens,cancels;} F;
static void logfn(void*p,int n,const char*s,...){(void)p;(void)n;(void)s;}
static int snap(void*v,uint32_t*g){F*f=v;*g=f->gen;return f->seen;}
static unsigned take(void*v){return mu_bt_trace_acl_take(&((F*)v)->latch);}
static int owner(void*v,uint32_t g){(void)v;return g!=0;}
static int pair(void*v,uint32_t g){F*f=v;return f->seen==1&&f->gen==g;}
static int idle(void*v){return ((F*)v)->wired;}
static int link(void*v,uint32_t g){return pair(v,g)&&((F*)v)->link;}
static int sdp(void*v,uint32_t g){return pair(v,g);}
static int open_it(void*v,uint32_t g){F*f=v;assert(pair(v,g));f->opens++;return 1;}
static int cancel(void*v,uint32_t g){F*f=v;assert(g);f->cancels++;return f->cancel;}
static int released(void*v,uint32_t g){(void)g;return ((F*)v)->release;}
int main(void){
 F f={0};MuPairedStart s;f.gen=7;f.seen=1;f.wired=1;f.cancel=1;f.release=1;
 struct cp_wcp_bt_ops ops={&f,owner,pair,idle,link,sdp,open_it,cancel};
 assert(mu_bt_trace_latch_bind_deferred(&f.latch,logfn));
 assert(mu_paired_start_init(&s,&ops,snap,take,released)==CP_WCP_OK);
 /* Already connected before any trace, no re-pair. */
 assert(mu_paired_start_tick(&s,0)==CP_WCP_OK);
 assert(mu_paired_start_tick(&s,4000)==CP_WCP_WAIT&&f.opens==1);
 mu_bt_trace_acl_observe(&f.latch,0x13f560,0x1234,0x285d88,0x1234);
 f.link=1;assert(mu_paired_start_tick(&s,4001)==CP_WCP_LINK_READY);
 /* Wired wins; uncertain teardown cannot be followed by a new peer. */
 f.wired=0;f.cancel=0;assert(mu_paired_start_tick(&s,4002)==CP_WCP_WAIT);
 assert(s.bt.state==CP_WCP_RESTORE_PENDING);
 f.wired=1;f.gen=8;f.link=0;assert(mu_paired_start_tick(&s,4003)==CP_WCP_WAIT&&f.opens==1);
 f.cancel=1;assert(mu_paired_start_tick(&s,4004)==CP_WCP_OK);
 f.release=0;assert(mu_paired_start_tick(&s,4005)==CP_WCP_DENIED);
 f.release=1;assert(mu_paired_start_tick(&s,4006)==CP_WCP_OK);
 /* Trace alone cannot authorize new connection. */
 f.seen=-1;mu_bt_trace_acl_observe(&f.latch,0x13f6c8,0x1234,0x285d44,0x1234);
 assert(mu_paired_start_tick(&s,8006)==CP_WCP_OK&&f.opens==1);
 assert(mu_paired_start_tick(&s,8007)==CP_WCP_WAIT);
 puts("PAIRED_START_EXISTING_LATCH_AND_AUTOCONNECT_PASS_FIXTURE_ONLY");
}
