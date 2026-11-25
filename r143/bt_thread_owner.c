/* R143: origin tracking for only the four hash-pinned MU1005 btstack callers.
 * No BlueSDK calls, no RFCOMM registration, no iAP2/MFi state changes. */
#define _POSIX_C_SOURCE 200809L
#include "bt_thread_owner.h"
#include <stdlib.h>
#include <string.h>
#define AU97_SLOTS 16
typedef struct { pthread_t thread; uintptr_t site; unsigned generation; unsigned state; } Au97Slot;
typedef struct { unsigned index, generation; void *(*start)(void *); void *arg; } Au97Launch;
static pthread_mutex_t state_lock=PTHREAD_MUTEX_INITIALIZER;
static Au97Slot slots[AU97_SLOTS];
static Au97Stats counters;
static const uintptr_t stock_sites[4]={0x23666cu,0x2368ccu,0x236c30u,0x236c4cu};
int au97_is_stock_pthread_caller(uintptr_t ra){unsigned i;ra&=~(uintptr_t)1;for(i=0;i<4;i++)if(ra==stock_sites[i])return 1;return 0;}
static void finish(void *p){Au97Launch *ctx=p;(void)pthread_mutex_lock(&state_lock);if(ctx->index<AU97_SLOTS&&slots[ctx->index].generation==ctx->generation&&slots[ctx->index].state){if(slots[ctx->index].state==2&&counters.running)counters.running--;slots[ctx->index].state=0;counters.finished++;}(void)pthread_mutex_unlock(&state_lock);free(ctx);}
static void *thread_start(void *p){Au97Launch *ctx=p;void *v=NULL;(void)pthread_mutex_lock(&state_lock);if(ctx->index<AU97_SLOTS&&slots[ctx->index].generation==ctx->generation&&slots[ctx->index].state==1){slots[ctx->index].thread=pthread_self();slots[ctx->index].state=2;counters.running++;}(void)pthread_mutex_unlock(&state_lock);pthread_cleanup_push(finish,ctx);v=ctx->start(ctx->arg);pthread_cleanup_pop(1);return v;}
int au97_pthread_create(pthread_t *t,const pthread_attr_t *a,void *(*start)(void *),void *arg,uintptr_t caller,Au97PthreadCreate orig){unsigned i,gen;Au97Launch *ctx;int rc;if(!orig)return -1;if(!start||!au97_is_stock_pthread_caller(caller))return orig(t,a,start,arg);ctx=calloc(1,sizeof(*ctx));if(!ctx){pthread_mutex_lock(&state_lock);counters.fallback++;pthread_mutex_unlock(&state_lock);return orig(t,a,start,arg);}pthread_mutex_lock(&state_lock);for(i=0;i<AU97_SLOTS;i++)if(!slots[i].state)break;if(i==AU97_SLOTS){counters.fallback++;pthread_mutex_unlock(&state_lock);free(ctx);return orig(t,a,start,arg);}gen=slots[i].generation+1;if(!gen)gen=1;slots[i].generation=gen;slots[i].state=1;slots[i].site=caller&~(uintptr_t)1;pthread_mutex_unlock(&state_lock);ctx->index=i;ctx->generation=gen;ctx->start=start;ctx->arg=arg;rc=orig(t,a,thread_start,ctx);pthread_mutex_lock(&state_lock);if(rc!=0){if(slots[i].generation==gen&&slots[i].state==1)slots[i].state=0;counters.failed++;}else{counters.issued++;if(t&&slots[i].generation==gen&&slots[i].state)slots[i].thread=*t;}pthread_mutex_unlock(&state_lock);if(rc)free(ctx);return rc;}
int au97_current_observed_site(uintptr_t site){pthread_t self=pthread_self();int ok=0;unsigned i;site&=~(uintptr_t)1;pthread_mutex_lock(&state_lock);for(i=0;i<AU97_SLOTS;i++)if(slots[i].state==2&&(site==0||site==slots[i].site)&&pthread_equal(self,slots[i].thread)){ok=1;break;}pthread_mutex_unlock(&state_lock);return ok;}
int au97_current_observed_thread(void){return au97_current_observed_site(0);}
int au97_is_observed_thread(pthread_t t){int ok=0;unsigned i;pthread_mutex_lock(&state_lock);for(i=0;i<AU97_SLOTS;i++)if(slots[i].state==2&&pthread_equal(t,slots[i].thread)){ok=1;break;}pthread_mutex_unlock(&state_lock);return ok;}
void au97_stats(Au97Stats *out){if(!out)return;pthread_mutex_lock(&state_lock);memcpy(out,&counters,sizeof(*out));pthread_mutex_unlock(&state_lock);}
