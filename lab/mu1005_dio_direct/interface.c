/* SPDX-License-Identifier: GPL-3.0-only
 * Direct original receiver interface adaptation, inspired by Congo.
 * NO INSTALL: only explicitly designated wireless DIO process.
 * Leave original USB path, receiver, authentication, and all other
 * properties untouched. No additional server or synthetic readiness. */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "sha256.h"
extern char *__progname;
#define STOCK "/eso/bin/apps/dio_manager"
#define STOCK_PIN "e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c"
#define UTF8 0x08000100u
typedef int (*Set)(void*,unsigned,const void*,const void*,const void*);
static Set original;
static unsigned long (*type)(const void*);
static unsigned long (*string_type)(void);
static void *(*make)(const void*,const char*,unsigned);
static unsigned char (*get)(const void*,char*,long,unsigned);
static void (*release)(const void*);
static pthread_once_t once=PTHREAD_ONCE_INIT;
static int allowed;
static const struct{uint32_t site,word;} sites[]={
#include "dio_property_sites.inc"
};
static int pin(void){
 unsigned char b[8192],d[32];char hex[65];static const char chars[]="0123456789abcdef";
 FILE*f=fopen(STOCK,"rb");if(!f)return 0;
 sha256_ctx ctx;size_t n;sha256_init(&ctx);
 while((n=fread(b,1,sizeof b,f)))sha256_update(&ctx,b,n);
 int err=ferror(f);if(fclose(f))err=1;if(err)return 0;
 sha256_final(&ctx,d);for(unsigned i=0;i<32;i++){hex[2*i]=chars[d[i]>>4];hex[2*i+1]=chars[d[i]&15];}hex[64]=0;
 return strcmp(hex,STOCK_PIN)==0;
}
static int resolve(const char*n,void*out,size_t size){
 if(size!=sizeof(void*))return 0;
 (void)dlerror();void*p=dlsym(RTLD_NEXT,n);if(!p||dlerror())return 0;memcpy(out,&p,size);return 1;
}
static void init(void){
 if(!resolve("AirPlayReceiverServerSetProperty",&original,sizeof original))return;
 const char*m=getenv("WCP_DIO_WIRELESS");
 const char*review=getenv("MU1005_DIO_INTERFACE_REVIEW");
 /* No opt-in from an ordinary OEM USB invocation. */
 if(!m||strcmp(m,"1")||!review||strcmp(review,"1")||!__progname||strcmp(__progname,"dio_manager")||!pin())return;
 if(!resolve("CFGetTypeID",&type,sizeof type)||!resolve("CFStringGetTypeID",&string_type,sizeof string_type)||
    !resolve("CFStringCreateWithCString",&make,sizeof make)||!resolve("CFStringGetCString",&get,sizeof get)||
    !resolve("CFRelease",&release,sizeof release))return;
 allowed=1;
}
static int caller(uintptr_t lr){
 if(lr<4||lr>UINT32_MAX||(lr&3u))return 0;
 uint32_t site=(uint32_t)lr-4u;
 for(unsigned i=0;i<sizeof sites/sizeof sites[0];i++)if(sites[i].site==site)
  return *(const volatile uint32_t*)(uintptr_t)site==sites[i].word;
 return 0;
}
static int equal(const void*x,const char*value){
 char b[32]={0};
 return x&&type(x)==string_type()&&get(x,b,sizeof b,UTF8)&&memchr(b,0,sizeof b)&&!strcmp(b,value);
}
static int ap_up(void){
 int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return 0;
 struct ifreq r;memset(&r,0,sizeof r);memcpy(r.ifr_name,"uap0",5);
 int ok=ioctl(fd,SIOCGIFFLAGS,&r)==0&&(r.ifr_flags&IFF_UP);
 memset(&r,0,sizeof r);memcpy(r.ifr_name,"uap0",5);
 ok=ok&&ioctl(fd,SIOCGIFADDR,&r)==0&&r.ifr_addr.sa_family==AF_INET;
 close(fd);return ok;
}
int AirPlayReceiverServerSetProperty(void*s,unsigned flags,const void*k,const void*q,const void*v){
 uintptr_t lr=(uintptr_t)__builtin_return_address(0);
 (void)pthread_once(&once,init);
 if(!original)return -1;
 /* Only redirect an actual stock request to bind carplay0. No call to
  * startServer, no replacement of other settings, no authentication changes.
  * A dedicated launch supervisor must still exclude concurrent wired DIO. */
 if(!allowed||!caller(lr)||!s||q||!equal(k,"interfaceName")||!equal(v,"carplay0")||!ap_up())
  return original(s,flags,k,q,v);
 void*ap=make(NULL,"uap0",UTF8);if(!ap)return original(s,flags,k,q,v);
 int rc=original(s,flags,k,q,ap);release(ap);
 return rc;
}
