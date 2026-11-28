/* SPDX-License-Identifier: GPL-3.0-only
 * One-shot parked-vehicle observation, not a lease or authorization.
 * No debug stop, injection, raw memory dump, or native SDK calls.
 */
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/mman.h>
#include <devctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt_startup_snapshot.h"
#include "bt_owner_identity.h"
#include "sha256.h"

typedef struct {int fd;procfs_mapinfo map[1024];unsigned reads;} Reader;
static int stock_path(const char *p){
 return !strcmp(p,"/eso/bin/apps/btstack")||!strcmp(p,"/mnt/app/eso/bin/apps/btstack");
}
static int digest_stock(const char *p){
 static const char want[]="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566";
 static const char hex[]="0123456789abcdef";
 unsigned char buf[8192],hash[32];char got[65];sha256_ctx h;
 struct stat a,b;int fd=open(p,O_RDONLY|O_NONBLOCK);
 if(fd<0)return 0;
 int ok=0;ssize_t n;size_t total=0;
 if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_size<=0||a.st_size>16000000)goto done;
 sha256_init(&h);
 while((n=read(fd,buf,sizeof buf))>0){total+=(size_t)n;if(total>(size_t)a.st_size)goto done;sha256_update(&h,buf,(size_t)n);}
 if(n!=0||total!=(size_t)a.st_size||fstat(fd,&b)||a.st_size!=b.st_size||a.st_ino!=b.st_ino||a.st_dev!=b.st_dev||a.st_mtime!=b.st_mtime||a.st_ctime!=b.st_ctime)goto done;
 sha256_final(&h,hash);for(unsigned i=0;i<32;i++){got[2*i]=hex[hash[i]>>4];got[2*i+1]=hex[hash[i]&15];}got[64]=0;ok=!strcmp(got,want);
done: close(fd);return ok;
}
static int executable_path(int fd,char *p,size_t cap){
 union {procfs_debuginfo info;unsigned char storage[sizeof(procfs_debuginfo)+1024];} d;
 memset(&d,0,sizeof d);
 if(devctl(fd,DCMD_PROC_MAPDEBUG_BASE,&d,sizeof d,NULL)!=EOK)return 0;
 size_t off=(size_t)((char*)d.info.path-(char*)&d);
 if(off>=sizeof d)return 0;
 const char *s=(char*)&d+off;
 const char *end=memchr(s,0,sizeof d-off);
 if(!end||(size_t)(end-s)>=cap||!stock_path(s))return 0;
 memcpy(p,s,(size_t)(end-s)+1);return 1;
}
static int readable(Reader*r,uint32_t a,size_t n){
 int count=0;
 if(devctl(r->fd,DCMD_PROC_MAPINFO,r->map,sizeof r->map,&count)!=EOK||count<=0||count>=1024)return 0;
 for(int i=0;i<count;i++){
  const procfs_mapinfo*m=&r->map[i];uint64_t start=m->vaddr,len=m->size;
  if((m->flags&PROT_READ)&&!(m->flags&PROT_NOCACHE)&&!(m->flags&MAP_PHYS)&&
      (uint64_t)a>=start&&(uint64_t)a-start<len&&n<=len-((uint64_t)a-start))return 1;
 }
 return 0;
}
/* Only used by the two existing pinned snapshot readers. No arbitrary
 * caller addresses, no writes, no output of memory, no repeated polling. */
static int read_observation(void*ctx,uint32_t a,void*out,size_t n){
 Reader*r=ctx;unsigned char b[256];off_t pos=(off_t)a;
 if(!out||!a||!n||n>sizeof b||++r->reads>160||(uint64_t)a+n>UINT64_C(0x100000000)||
    pos<0||(uint64_t)pos!=a||!readable(r,a,n))return 0;
 if(lseek(r->fd,pos,SEEK_SET)!=pos||read(r->fd,b,n)!=(ssize_t)n||!readable(r,a,n))return 0;
 memcpy(out,b,n);memset(b,0,sizeof b);return 1;
}
int main(int argc,char**argv){
 if(argc!=2||!argv[1][0]||strspn(argv[1],"0123456789")!=strlen(argv[1])||strlen(argv[1])>10)return 2;
 char path[64],exe[128],again[128];char*end;unsigned long pid=strtoul(argv[1],&end,10);
 if(*end||pid<2||pid>2147483647ul)return 2;
 snprintf(path,sizeof path,"/proc/%lu/as",pid);
 Reader*r=calloc(1,sizeof *r);if(!r)return 2;
 r->fd=open(path,O_RDONLY|O_NONBLOCK);if(r->fd<0){puts("BTSTACK_PROC_UNAVAILABLE");free(r);return 2;}
 int identity=executable_path(r->fd,exe,sizeof exe)&&digest_stock(exe);
 int anchors=0,started=0,libs=0,ao=0,stable=0;
 uint32_t service=0,u=0,m=0;MuBtThreadIdentity t={0};
 if(identity){
  anchors=mu_bt_sdk_lifecycle_sites(read_observation,r);
  if(anchors){
   service=mu_bt_sdk_service_address(read_observation,r);started=service!=0;
   if(started){
    libs=mu_bt_library_biases(read_observation,r,&u,&m);
    if(libs)ao=mu_bt_osapi_target(m,read_observation,r,&t);
    if(ao)stable=mu_bt_osapi_identity_matches(&t,t.tid,read_observation,r)&&mu_bt_sdk_service_address(read_observation,r)==service;
   }
  }
 }
 if(!executable_path(r->fd,again,sizeof again)||!identity||strcmp(exe,again))identity=anchors=started=libs=ao=stable=0;
 printf("BTSTACK_EXACT_STOCK=%d\nBTSTACK_LIFECYCLE_ANCHORS=%d\nBTSTACK_SERVICE_STARTED=%d\nBTSTACK_LIBRARY_ANCHORS=%d\nBTSTACK_AO_IDENTIFIED=%d\nBTSTACK_SNAPSHOT_STABLE=%d\n",identity,anchors,started,libs,ao,stable);
 puts("OBSERVATION_ONLY_NO_OWNER_LEASE_NO_INSTALL");close(r->fd);free(r);return identity?0:2;
}
