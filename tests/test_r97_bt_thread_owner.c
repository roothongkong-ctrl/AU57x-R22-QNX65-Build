/* R97 host proof: REAL pthreads, origin/ownership lifetime, no OEM callbacks. */
#define _POSIX_C_SOURCE 200809L
#include "bt_thread_owner.h"
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t c;
    int ready;
    int release;
    int observed;
    int specific;
    uintptr_t expected_site;
    int exit_direct;
} Gate;
static void gate_init(Gate*g,uintptr_t site,int exit_direct){
    memset(g,0,sizeof(*g));g->expected_site=site;g->exit_direct=exit_direct;
    assert(pthread_mutex_init(&g->m,NULL)==0);
    assert(pthread_cond_init(&g->c,NULL)==0);
}
static void gate_wait(Gate *g){
    assert(pthread_mutex_lock(&g->m)==0);
    while(!g->ready)assert(pthread_cond_wait(&g->c,&g->m)==0);
    assert(pthread_mutex_unlock(&g->m)==0);
}
static void gate_release(Gate*g){
    assert(pthread_mutex_lock(&g->m)==0);
    g->release=1;
    assert(pthread_cond_broadcast(&g->c)==0);
    assert(pthread_mutex_unlock(&g->m)==0);
}
static void *worker(void*p){
    Gate*g=(Gate*)p;
    g->observed=au97_current_observed_thread();
    g->specific=au97_current_observed_site(g->expected_site);
    assert(pthread_mutex_lock(&g->m)==0);
    g->ready=1;
    assert(pthread_cond_broadcast(&g->c)==0);
    while(!g->release)assert(pthread_cond_wait(&g->c,&g->m)==0);
    assert(pthread_mutex_unlock(&g->m)==0);
    if(g->exit_direct)pthread_exit((void*)0x17);
    return (void*)0x19;
}
static void join_gate(pthread_t t,Gate*g,int is_exit){
    void* result=NULL;
    gate_release(g);
    assert(pthread_join(t,&result)==0);
    assert(result==(void*)(uintptr_t)(is_exit?0x17:0x19));
    assert(pthread_cond_destroy(&g->c)==0);
    assert(pthread_mutex_destroy(&g->m)==0);
}
static int broken_create(pthread_t*t,const pthread_attr_t*a,void*(*start)(void*),void*arg){
    (void)t;(void)a;(void)start;(void)arg;
    return EAGAIN;
}
int main(void){
    const uintptr_t sites[]={0x23666c,0x2368cc,0x236c30,0x236c4c};
    Au97Stats before,after;
    Gate g;pthread_t t;
    unsigned i;
    assert(!au97_current_observed_thread());
    assert(!au97_is_stock_pthread_caller(0x2464b4));
    assert(au97_is_stock_pthread_caller(0x23666d));
    for(i=0;i<4;i++)assert(au97_is_stock_pthread_caller(sites[i]));
    au97_stats(&before);
    for(i=0;i<4;i++){
        gate_init(&g,sites[i],i==1);
        assert(au97_pthread_create(&t,NULL,worker,&g,sites[i],pthread_create)==0);
        gate_wait(&g);
        assert(g.observed==1 && g.specific==1);
        assert(!au97_current_observed_thread());
        au97_stats(&after);assert(after.running==1);
        join_gate(t,&g,i==1);
        au97_stats(&after);assert(after.running==0);
    }
    assert(after.issued==before.issued+4 && after.finished==before.finished+4);
    /* A random caller must not be classified as a stock event thread. */
    gate_init(&g,0x23666c,0);
    assert(au97_pthread_create(&t,NULL,worker,&g,0x223344,pthread_create)==0);
    gate_wait(&g);
    assert(!g.observed && !g.specific);
    join_gate(t,&g,0);
    au97_stats(&after);assert(after.issued==before.issued+4);
    /* A failed OS create must never leave a pending OEM slot. */
    assert(au97_pthread_create(&t,NULL,worker,&g,sites[0],broken_create)==EAGAIN);
    au97_stats(&after);assert(after.failed==before.failed+1 && after.running==0);
    /* Busy slots: the OEM call must still be forwarded unmodified, not fail. */
    {
      Gate gates[17];pthread_t threads[17];
      for(i=0;i<17;i++) {
         gate_init(&gates[i],sites[0],0);
         assert(au97_pthread_create(&threads[i],NULL,worker,&gates[i],sites[0],pthread_create)==0);
         gate_wait(&gates[i]);
         assert(gates[i].observed==(i<16));
      }
      for(i=0;i<17;i++)join_gate(threads[i],&gates[i],0);
      au97_stats(&after);
      assert(after.running==0 && after.fallback>=before.fallback+1);
    }
    /* The same slot may be re-used only by its current generation. */
    gate_init(&g,sites[3],0);
    assert(au97_pthread_create(&t,NULL,worker,&g,sites[3],pthread_create)==0);
    gate_wait(&g);assert(g.observed);
    join_gate(t,&g,0);
    au97_stats(&after);assert(after.running==0);
    assert(au97_pthread_create(&t,NULL,worker,&g,sites[0],NULL)==-1);
    puts("R97 ORIGINAL-BTSTACK THREAD ORIGIN: 4 stock sites + wrong-site + pthread_exit + failure + 16-slot backpressure + reuse PASS (HOST fixture)");
    return 0;
}
