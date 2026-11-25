/* SPDX-License-Identifier: GPL-3.0-only */
#include "mm_notify.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
static unsigned char dev[128],drv[128],transport[128],other[128];
static int allowed=1,calls;
static MuMmNotify n;
int transport_recv_pkt(void *p){
 assert(p==drv);calls++;
 assert(mu_mm_notify_input(&n,dev)==-1);return 0;
}
static void put(unsigned char *p,unsigned off,void *v){memcpy(p+off,&v,sizeof v);}
static int proof(void *ctx,void *d,void *r,void *t,uint64_t e){
 return ctx==&n&&d==dev&&r==drv&&t==transport&&e==42&&allowed;
}
int main(void){
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 put(dev,0x38,drv);put(dev,0x40,transport);put(drv,0x14,dev);
 void *fn=dlsym(h,"transport_recv_pkt");assert(fn);
 put(drv,0x20,fn);
 assert(mu_mm_notify_bind(&n,h,dev,drv,transport,42,&n,proof)==1);
 assert(mu_mm_notify_input(&n,dev)==1&&calls==1);
 put(dev,0x40,other);assert(mu_mm_notify_input(&n,dev)==-1&&calls==1);
 put(dev,0x40,transport);put(drv,0x14,other);
 assert(mu_mm_notify_input(&n,dev)==-1&&calls==1);
 put(drv,0x14,dev);put(drv,0x20,NULL);
 assert(mu_mm_notify_input(&n,dev)==-1&&calls==1);
 put(drv,0x20,fn);allowed=0;
 assert(mu_mm_notify_input(&n,dev)==-1&&calls==1);
 mu_mm_notify_clear(&n);assert(!n.bound);dlclose(h);
 puts("MU1005_MM_NOTIFY_SLOT_HOST_PASS_NO_OEM");return 0;
}
