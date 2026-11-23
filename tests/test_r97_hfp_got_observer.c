#define _POSIX_C_SOURCE 200809L
#include "bt_got_observer.h"
#include "bt_thread_owner.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

typedef void (*Cb)(void*,const void*);
static volatile unsigned original_calls,original_last_code;
static void original_cb(void*ch,const void*e){
    const unsigned char*evt=e;
    assert(ch==(void*)0x24);
    original_calls++;
    original_last_code=evt?evt[0]:0;
}
static void replacement_cb(void*ch,const void*e){(void)ch;(void)e;}
static Cb as_cb(uintptr_t bits){Cb x=NULL;memcpy(&x,&bits,sizeof x);return x;}
static void *owner_worker(void *ctx){
    volatile uintptr_t *s=(volatile uintptr_t*)ctx;
    unsigned char evt[16]={5};
    assert(au97_current_observed_thread()==1);
    as_cb(*s)((void*)0x24,evt);
    return NULL;
}
int main(void){
    Au97GotHook tmp={0};Au97GotStats s={0};
    volatile uintptr_t slot=(uintptr_t)&original_cb;
    unsigned char evt[16]={1};pthread_t worker;
    assert(au97_got_replace(NULL,&slot,(uintptr_t)&original_cb,(uintptr_t)&replacement_cb)<0);
    assert(au97_got_replace(&tmp,&slot,0,(uintptr_t)&replacement_cb)<0);
    assert(au97_got_replace(&tmp,&slot,(uintptr_t)&replacement_cb,(uintptr_t)&replacement_cb)<0);
    assert(au97_got_replace(&tmp,&slot,(uintptr_t)&replacement_cb,(uintptr_t)&original_cb)<0);
    assert(slot==(uintptr_t)&original_cb);
    assert(au97_test_install_hfp(&slot,(uintptr_t)&original_cb)==0);
    assert(au97_test_install_hfp(&slot,(uintptr_t)&original_cb)<0);
    assert(slot!=(uintptr_t)&original_cb);
    as_cb(slot)((void*)0x24,evt);
    evt[0]=2;as_cb(slot)((void*)0x24,evt);
    evt[0]=4;as_cb(slot)((void*)0x24,evt);
    evt[0]=6;as_cb(slot)((void*)0x24,evt);
    au97_hfp_observer_stats(&s);
    assert(original_calls==4&&original_last_code==6);
    assert(s.event_seen[1]==1&&s.event_seen[2]==1&&s.event_seen[4]==1&&s.event_seen[6]==1);
    assert(s.on_unknown_thread==4&&s.on_observed_thread==0&&s.active_callbacks==0);
    /* A thread created from a pinned original callsite is distinguishable. */
    assert(au97_pthread_create(&worker,NULL,owner_worker,(void*)&slot,0x23666c,pthread_create)==0);
    assert(pthread_join(worker,NULL)==0);
    au97_hfp_observer_stats(&s);
    assert(s.on_observed_thread==1&&s.event_seen[5]==1&&original_calls==5);
    assert(au97_install_original_hfp_observer()<0); /* impossible in host fixture */
    assert(au97_got_restore_quiesced(&tmp,1)<0);
    puts("R97 HFP GLOB_DAT passive forwarding + 5 callback codes + authenticated thread origin HOST fixture PASS");
    return 0;
}
