/* SPDX-License-Identifier: GPL-3.0-only
 * MU1005 first-link trace attachment candidate: original trace call retained.
 * QNX ARM only; never changes executable mappings or performs SDK calls.
 * Opt-in before original startup; a resolved writable GOT slot is required.
 */
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include "bt_trace_latch.h"
#include "bt_startup_snapshot.h"
#include "bt_self_reader.h"
#include "bt_got_observer.h"
#include "bt_preload_probe.h"
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include <dlfcn.h>
#include <devctl.h>
#include <sys/procfs.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
static MuBtTraceLatch latch;
static Au97GotHook trace_hook;
static volatile unsigned attach_busy;
static volatile unsigned attach_done;
static volatile unsigned acl_observer_ready;
static uintptr_t bound_original;
extern void mu_bt_trace_tail(void*,int,const char*,...);
__attribute__((visibility("hidden")))
MuBtTraceOriginal mu_bt_trace_target(void *channel,uintptr_t lr,const char *fmt){
 /* The fixed slot is in the pinned resident main executable and was
  * checked readable before hook publication. Only inspect it for the exact
  * startup call. No procfs IPC, loader calls, locks or OEM SDK calls here. */
 uintptr_t current=0;
 if(lr==0x11ed14u&&(uintptr_t)fmt==0x27cf94u)
  current=__atomic_load_n((const uint32_t*)(uintptr_t)0x2c18fcu,__ATOMIC_ACQUIRE);
 /* Same original trace forwarder; never change threshold or decode the
  * private variadic peer address. The worker must query the paired owner. */
 if(__sync_val_compare_and_swap(&acl_observer_ready,1u,1u)==1u&&
    ((lr==0x13f6c8u&&(uintptr_t)fmt==0x285d44u)||
     (lr==0x13f1dcu&&(uintptr_t)fmt==0x285bb0u)||
     (lr==0x13f560u&&(uintptr_t)fmt==0x285d88u))){
  uintptr_t acl=__atomic_load_n((const uint32_t*)(uintptr_t)0x2c2258u,__ATOMIC_ACQUIRE);
  mu_bt_trace_acl_observe(&latch,lr,(uintptr_t)channel,(uintptr_t)fmt,acl);
 }
 return mu_bt_trace_latch_observe_live(&latch,lr,(uintptr_t)channel,(uintptr_t)fmt,current);
}
static int allow(void *unused,uint32_t address,size_t n){
 (void)unused;
 return ((address==0x2c2258u||address==0x13f6c4u||address==0x13f55cu||address==0x13f1d8u)&&n==4)||
        (address==0x2c12c8u&&n==4)||(address==0x2c18fcu&&n==4)||
        (address==0x11ed10u&&n==4)||(address==0x27cf94u&&n==sizeof("%s(%d): Primary DID registered"));
}
static int writable_slot(void){
 procfs_mapinfo maps[512];int count=0;char path[64];
 int n=snprintf(path,sizeof path,"/proc/%ld/as",(long)getpid());
 if(n<0||(size_t)n>=sizeof path)return 0;
 int fd=open(path,O_RDONLY);if(fd<0)return 0;
 int rc=devctl(fd,DCMD_PROC_MAPINFO,maps,sizeof maps,&count);close(fd);
 if(rc!=EOK||count<1||count>=512)return 0;
 for(int i=0;i<count;i++){
  uint64_t start=maps[i].vaddr,len=maps[i].size,a=0x2c12c8u;
  if(a>=start&&a-start<len&&4<=len-(a-start)&&
    (maps[i].flags&PROT_READ)&&(maps[i].flags&PROT_WRITE)&&
    !(maps[i].flags&PROT_EXEC)&&!(maps[i].flags&MAP_PHYS)&&
    !(maps[i].flags&PROT_NOCACHE))return 1;
 }
 return 0;
}
static int attach_once(void){
 if(!au57x_r92_stock_image_verified()||!writable_slot())return 0;
 void *symbol=dlsym(RTLD_NEXT,"_ZN7tracing7Channel5logCVEsPKcz");
 uintptr_t resolved=0;MuBtTraceOriginal original=NULL;
 if(!symbol)return 0;
 memcpy(&original,&symbol,sizeof original);
 memcpy(&resolved,&symbol,sizeof resolved);
 MuBtSelfReader *r=mu_bt_self_reader_open(allow,NULL);
 if(!r)return 0;
 uint32_t channel=0,slot=0;
 int ok=mu_bt_self_reader_read(r,0x2c18fcu,&channel,4)&&
    mu_bt_self_reader_read(r,0x2c12c8u,&slot,4)&&slot==resolved&&
    mu_bt_sdk_trace_site(mu_bt_self_reader_read,r);
 /* Validate all three exact ARM BL words and readable live GOT slot.
  * R105.232 relocation identifies JOBS_STARTSERVICE, matching Congo
  * pinned reference table. If anything differs, keep only startup. */
 uint32_t acl=0,est=0,lost=0,already=0;
 int acl_ok=mu_bt_self_reader_read(r,0x2c2258u,&acl,4)&&
    mu_bt_self_reader_read(r,0x13f6c4u,&est,4)&&est==0xebff5ddeu;
 acl_ok=acl_ok&&mu_bt_self_reader_read(r,0x13f55cu,&lost,4)&&lost==0xebff5e38u&&
    mu_bt_self_reader_read(r,0x13f1d8u,&already,4)&&already==0xebff5f19u;
 mu_bt_self_reader_close(r);
 /* Zero channel before C++ initialization is valid at attachment time.
  * The actual call must match the then-current nonzero stock slot. */
 if(!ok)return 0;
 if(!bound_original){
  if(!mu_bt_trace_latch_bind_deferred(&latch,original))return 0;
  bound_original=resolved;
 }else if(bound_original!=resolved)return 0;
 /* Publish the tail only after original forwarding has been bound.
  * If lazy resolution or a competing slot change is present, do not attach. */
 if(acl_ok)(void)__sync_lock_test_and_set(&acl_observer_ready,1u);
 return au97_got_replace(&trace_hook,(volatile uintptr_t*)0x2c12c8u,
                         resolved,(uintptr_t)&mu_bt_trace_tail)==0;
}
/* Constructor may run before the stock GOT has been relocated. Retry only
 * at already identified stock thread creation sites; serialize publication.
 * A failed compare/exchange leaves the original target bound for retry. */
int mu_bt_trace_attach(void){
 if(__sync_val_compare_and_swap(&attach_done,1u,1u)==1u)return 1;
 if(!__sync_bool_compare_and_swap(&attach_busy,0u,1u))return 0;
 int ok=__sync_val_compare_and_swap(&attach_done,1u,1u)==1u;
 if(!ok && attach_once()){
  __sync_synchronize();
  (void)__sync_bool_compare_and_swap(&attach_done,0u,1u);
  ok=1;
 }
 __sync_lock_release(&attach_busy);
 return ok;
}
int mu_bt_trace_attached_take(void){return __sync_val_compare_and_swap(&attach_done,1u,1u)==1u?mu_bt_trace_latch_take(&latch):0;}
unsigned mu_bt_trace_attached_acl_take(void){
 return __sync_val_compare_and_swap(&attach_done,1u,1u)==1u?mu_bt_trace_acl_take(&latch):0u;
}
#else
unsigned mu_bt_trace_attached_acl_take(void){return 0u;}
int mu_bt_trace_attach(void){return 0;}
int mu_bt_trace_attached_take(void){return 0;}
#endif
