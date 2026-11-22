/* SPDX-License-Identifier: GPL-3.0-only
 * R92: actual QNX 6.5 LD_PRELOAD entry probe, thread creation visibility.
 * No guessed vendor API calls. Never marks iAP2/MFi ready.
 * Pin the OEM executable bytes before enabling ANY instrumentation.
 */
#define _POSIX_C_SOURCE 200809L
#include "bt_preload_probe.h"
#include "../../../../tools/review_txn/sha256.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef AU57X_R92_STOCK_SHA256
#define AU57X_R92_STOCK_SHA256 "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
#endif
#define BTSTACK_STOCK_PATH "/eso/bin/apps/btstack"
#define BT_PROBE_LOG "/tmp/au57x_r92_bt_preload.log"
static volatile unsigned probe_enabled;
static volatile unsigned thread_count;
static const uint32_t stock_callers[4]={0x236668u,0x2368c8u,0x236c2cu,0x236c48u};

static int digest_hex(const char *path,char out[65]){
 unsigned char b[8192],d[32];static const char hex[]="0123456789abcdef";
 sha256_ctx ctx;size_t n,i;FILE *fp;
 if(!path)return 0;
 fp=fopen(path,"rb");if(!fp)return 0;
 sha256_init(&ctx);
 while((n=fread(b,1,sizeof b,fp))!=0)sha256_update(&ctx,b,n);
 if(ferror(fp)){(void)fclose(fp);return 0;}
 if(fclose(fp)!=0)return 0;
 sha256_final(&ctx,d);
 for(i=0;i<32;i++){out[2*i]=hex[d[i]>>4];out[2*i+1]=hex[d[i]&15];}
 out[64]=0;return 1;
}
static void log_one(const char *event,uintptr_t callsite,uintptr_t start,int matched){
 char line[256];int n,fd;
 /* A single syscall-style write; no stdio FILE pointer to race in callbacks. */
 n=snprintf(line,sizeof line,"R92 %s pid=%ld caller=0x%lx start=0x%lx stock_pc=%d\n",
 event,(long)getpid(),(unsigned long)callsite,(unsigned long)start,matched);
 if(n<=0||n>=(int)sizeof line)return;
 fd=open(BT_PROBE_LOG,O_WRONLY|O_CREAT|O_APPEND,0600);
 if(fd>=0){(void)write(fd,line,(size_t)n);(void)close(fd);}
}
static void log_failure(const char *event){
 /* No OEM API calls or log of sensitive MACs in a rejected process. */
 log_one(event,0,0,0);
}
__attribute__((constructor)) static void au57x_probe_ctor(void){
 char digest[65];const char *path=BTSTACK_STOCK_PATH;
 const char *opt=getenv("AU57X_R92_PROBE");
 if(!opt||strcmp(opt,"1")!=0)return;
#if defined(AU57X_R92_HOST_TEST) && AU57X_R92_HOST_TEST==1
 path=getenv("AU57X_R92_HOST_TARGET");
 if(!path)return;
#endif
 if(!digest_hex(path,digest) || strcmp(digest,AU57X_R92_STOCK_SHA256)!=0){
  log_failure("pin-reject");return;
 }
 /* Only an exact stock executable is admitted; no mutation or vendor calls. */
 probe_enabled=1;
 log_one("stock-image-pinned",0,0,1);
}
int au57x_r92_stock_image_verified(void){return probe_enabled==1;}
unsigned au57x_r92_threads_seen(void){return thread_count;}
/* Interpose an imported libc operation. Original OEM calls are forwarded
 * unmodified (same args, return code, thread entry, and thread identity).
 * This makes actual preload entry and stock thread creation observable.
 * It is NOT the BlueSDK owner callback and does NOT register RFCOMM.
 */
int pthread_create(pthread_t *t,const pthread_attr_t *attr,
                   void *(*start)(void*),void *arg){
 typedef int (*real_create_fn)(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
 real_create_fn orig;void *symbol=dlsym(RTLD_NEXT,"pthread_create");
 uintptr_t site=0;int matched=0;size_t i;
 /* Avoid ISO C object->function-pointer aliasing warning in strict builds. */
 if(!symbol)return -1;
 memcpy(&orig,&symbol,sizeof orig);
 if(probe_enabled){
  site=(uintptr_t)__builtin_return_address(0);
  /* ARM state BL return address = instruction VA+4; Thumb flag masked.
   * An unknown caller is logged as untrusted, never converted into owner. */
  for(i=0;i<sizeof stock_callers/sizeof stock_callers[0];i++)
    if((site&~(uintptr_t)1u)==(uintptr_t)stock_callers[i]+4u)matched=1;
  (void)__sync_fetch_and_add(&thread_count,1u);
  log_one("pthread_create",site,(uintptr_t)start,matched);
 }
 return orig(t,attr,start,arg);
}
