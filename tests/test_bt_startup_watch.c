/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_startup_watch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {unsigned held,allow,started,ready,reads,enters,leaves,bad;} F;
static void original(void*p,int n,const char*s,...){(void)p;(void)n;(void)s;}
static void put(void *p,uint32_t v){unsigned char *b=p;for(unsigned i=0;i<4;i++)b[i]=(unsigned char)(v>>(8*i));}
static int enter(void *p){F*f=p;assert(!f->held);f->enters++;return f->held=f->allow;}
static void leave(void*p){F*f=p;assert(f->held);f->held=0;f->leaves++;}
static void ready(void*p,uint32_t s){F*f=p;assert(f->held&&s==0x100000&&f->started);f->ready++;}
static int read_live(void*p,uint32_t a,void*out,size_t n){
 F*f=p;assert(f->held);f->reads++;
 static const uint32_t addr[]={0x2be500,0x11e9b0,0x11eab0,0x11ec94,0x11ec98,0x125908,0x125968,0x125970,0x11de34,0x11de54};
 static const uint32_t val[]={0x11e9b0,0xe92d4ff0,0xeb0571b6,0xe3a03001,0xe5c531b0,0xe92d4070,0xeb055604,0xe5c531b0,0xeb0574d1,0xe5c831b0};
 if(n==4){for(unsigned i=0;i<10;i++)if(a==addr[i]){put(out,val[i]^(f->bad&&i==4));return 1;}
 if(a==0x2c3d54)put(out,1);else if(a==0x2c3d48)put(out,0x10001c);else if(a==0x10001c)put(out,0x2be61c);else if(a==0x100000)put(out,0x2be500);else return 0;return 1;}
 if(a==0x1001b0&&n==1){*(unsigned char*)out=(unsigned char)f->started;return 1;}
 if(a==0x2d5644&&n==24){unsigned char *q=out;memset(q,0,n);put(q,0x2c10d8);for(unsigned i=1;i<=4;i++)put(q+i*4,0x400000+i*8);q[22]=1;return 1;}
 return 0;
}
static void hint(MuBtTraceLatch*l){assert(mu_bt_trace_latch_observe(l,0x11ed14,0x1234,0x27cf94)==original);}
static int tick(MuBtStartupWatch*w,MuBtTraceLatch*l,F*f){return mu_bt_startup_watch_tick(w,l,3,enter,leave,read_live,ready,f);}
void mu_bt_startup_watch_test(void){
 MuBtStartupWatch w={0};MuBtTraceLatch l={0};F f={0};
 assert(mu_bt_trace_latch_bind(&l,0x1234,original));
 assert(tick(&w,&l,&f)==0&&!f.enters);
 hint(&l);assert(!tick(&w,&l,&f)&&!f.reads&&w.remaining==2);
 f.allow=1;assert(!tick(&w,&l,&f)&&w.remaining==1&&!f.ready);
 f.started=1;assert(tick(&w,&l,&f)==1&&f.ready==1&&!f.held&&!w.remaining);
 assert(!tick(&w,&l,&f)&&f.ready==1);
 f.bad=1;hint(&l);assert(!tick(&w,&l,&f));hint(&l);assert(!tick(&w,&l,&f));assert(!tick(&w,&l,&f)&&!w.remaining);
 assert(f.ready==1&&f.leaves+1==f.enters&&!f.held);
 puts("MU1005_STARTUP_WATCH_LEASE_RETRY_PASS_FIXTURE_ONLY");
}
#ifdef STARTUP_WATCH_STANDALONE
int main(void){mu_bt_startup_watch_test();return 0;}
#endif
