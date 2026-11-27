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
extern void mu_bt_trace_tail(void*,int,const char*,...);
__attribute__((visibility("hidden")))
MuBtTraceOriginal mu_bt_trace_target(void *channel,uintptr_t lr,const char *fmt){
 return mu_bt_trace_latch_observe(&latch,lr,(uintptr_t)channel,(uintptr_t)fmt);
}
static int allow(void *unused,uint32_t address,size_t n){
 (void)unused;
 return (address==0x2c12c8u&&n==4)||(address==0x2c18fcu&&n==4)||
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
int mu_bt_trace_attach(void){
 if(!au57x_r92_stock_image_verified()||!writable_slot()||trace_hook.installed)return 0;
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
    mu_bt_sdk_start_trace(0x11ed14u,channel,0x27cf94u,mu_bt_self_reader_read,r);
 mu_bt_self_reader_close(r);
 if(!ok||!mu_bt_trace_latch_bind(&latch,channel,original))return 0;
 /* Publish the tail only after original forwarding has been bound.
  * If lazy resolution or a competing slot change is present, do not attach. */
 return au97_got_replace(&trace_hook,(volatile uintptr_t*)0x2c12c8u,
                         resolved,(uintptr_t)&mu_bt_trace_tail)==0;
}
int mu_bt_trace_attached_take(void){return trace_hook.installed?mu_bt_trace_latch_take(&latch):0;}
#else
int mu_bt_trace_attach(void){return 0;}
int mu_bt_trace_attached_take(void){return 0;}
#endif
