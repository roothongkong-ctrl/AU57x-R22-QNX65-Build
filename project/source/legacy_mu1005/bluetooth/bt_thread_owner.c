/* SPDX-License-Identifier: GPL-3.0-only
 * R97 actual pthread origin tracking. Production means OEM process entered
 * R92 preloader AND started a thread at a proven MU1005 callsite; no live
 * Bluetooth event, MAC or Apple/MFi authority is implied.
 */
#define _POSIX_C_SOURCE 200809L
#include "bt_thread_owner.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AU97_SLOTS 16
static const uintptr_t stock_sites[4] = {0x23666cu,0x2368ccu,0x236c30u,0x236c4cu};
typedef struct {
    pthread_t thread;
    uintptr_t site;
    unsigned generation;
    unsigned state; /* 0 free, 1 pending, 2 running */
} Au97Slot;
typedef struct {
    unsigned index;
    unsigned generation;
    void *(*start)(void *);
    void *arg;
} Au97Launch;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static Au97Slot slots[AU97_SLOTS];
static Au97Stats counters;

int au97_is_stock_pthread_caller(uintptr_t return_address) {
    unsigned i;
    uintptr_t site = return_address & ~(uintptr_t)1;
    for(i=0;i<4;i++)if(site==stock_sites[i])return 1;
    return 0;
}
static void finish(void *p) {
    Au97Launch *ctx = (Au97Launch*)p;
    (void)pthread_mutex_lock(&state_lock);
    if(ctx->index<AU97_SLOTS &&
       slots[ctx->index].generation==ctx->generation &&
       slots[ctx->index].state!=0) {
        if(slots[ctx->index].state==2 && counters.running) counters.running--;
        slots[ctx->index].state=0;
        counters.finished++;
    }
    (void)pthread_mutex_unlock(&state_lock);
    free(ctx);
}
static void *thread_start(void *p) {
    Au97Launch *ctx=(Au97Launch*)p;
    void *value=NULL;
    (void)pthread_mutex_lock(&state_lock);
    if(ctx->index<AU97_SLOTS &&
       slots[ctx->index].generation==ctx->generation &&
       slots[ctx->index].state==1) {
        slots[ctx->index].thread=pthread_self();
        slots[ctx->index].state=2;
        counters.running++;
    }
    (void)pthread_mutex_unlock(&state_lock);
    /* cleanup also runs on pthread_exit() and cancellation. */
    pthread_cleanup_push(finish,ctx);
    value=ctx->start(ctx->arg);
    pthread_cleanup_pop(1);
    return value;
}
int au97_pthread_create(pthread_t *thread,const pthread_attr_t *attr,
                        void *(*start)(void *),void *arg,uintptr_t caller,
                        Au97PthreadCreate orig) {
    unsigned i,gen;
    Au97Launch *ctx;
    int rc;
    if(!orig)return -1;
    if(!start||!au97_is_stock_pthread_caller(caller))return orig(thread,attr,start,arg);
    ctx=(Au97Launch*)calloc(1,sizeof(*ctx));
    if(!ctx) {
        (void)pthread_mutex_lock(&state_lock);counters.fallback++;
        (void)pthread_mutex_unlock(&state_lock);
        return orig(thread,attr,start,arg);
    }
    (void)pthread_mutex_lock(&state_lock);
    for(i=0;i<AU97_SLOTS;i++)if(slots[i].state==0)break;
    if(i==AU97_SLOTS) {
        counters.fallback++;
        (void)pthread_mutex_unlock(&state_lock);free(ctx);
        return orig(thread,attr,start,arg);
    }
    gen=slots[i].generation+1;
    if(gen==0)gen=1;
    slots[i].generation=gen;
    slots[i].state=1;
    slots[i].site=caller&~(uintptr_t)1;
    (void)pthread_mutex_unlock(&state_lock);
    ctx->index=i;ctx->generation=gen;ctx->start=start;ctx->arg=arg;
    rc=orig(thread,attr,thread_start,ctx);
    (void)pthread_mutex_lock(&state_lock);
    if(rc!=0) {
        if(slots[i].generation==gen&&slots[i].state==1)slots[i].state=0;
        counters.failed++;
    } else counters.issued++;
    (void)pthread_mutex_unlock(&state_lock);
    if(rc!=0)free(ctx);
    return rc;
}
int au97_current_observed_site(uintptr_t site) {
    pthread_t self=pthread_self();int valid=0;
    unsigned i;
    site&=~(uintptr_t)1;
    (void)pthread_mutex_lock(&state_lock);
    for(i=0;i<AU97_SLOTS;i++) {
        if(slots[i].state==2 && (site==0||site==slots[i].site) &&
           pthread_equal(self,slots[i].thread)) {valid=1;break;}
    }
    (void)pthread_mutex_unlock(&state_lock);
    return valid;
}
int au97_current_observed_thread(void){return au97_current_observed_site(0);}
void au97_stats(Au97Stats *out) {
    if(!out)return;
    (void)pthread_mutex_lock(&state_lock);
    memcpy(out,&counters,sizeof(*out));
    (void)pthread_mutex_unlock(&state_lock);
}
