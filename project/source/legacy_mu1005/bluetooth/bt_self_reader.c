/* SPDX-License-Identifier: GPL-3.0-only */
#define _FILE_OFFSET_BITS 64
#include "bt_self_reader.h"
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include <devctl.h>
#include <sys/procfs.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#define MAP_CAP 512
#define READ_CAP 4096
struct MuBtSelfReader{
 int fd;pid_t pid;pthread_mutex_t lock;
 int(*allow)(void*,uint32_t,size_t);void*ctx;
 procfs_mapinfo map[MAP_CAP];uint8_t temp[READ_CAP];
};
MuBtSelfReader*mu_bt_self_reader_open(int(*allow)(void*,uint32_t,size_t),void*ctx){
 if(!allow)return NULL;
 MuBtSelfReader*r=calloc(1,sizeof *r);if(!r)return NULL;
 char path[64];r->pid=getpid();
 int len=snprintf(path,sizeof path,"/proc/%ld/as",(long)r->pid);
 if(len<0||(size_t)len>=sizeof path){free(r);return NULL;}
 r->fd=open(path,O_RDONLY);
 if(r->fd<0){free(r);return NULL;}
 if(fcntl(r->fd,F_SETFD,FD_CLOEXEC)<0||pthread_mutex_init(&r->lock,NULL)!=0){
  close(r->fd);free(r);return NULL;
 }
 r->allow=allow;r->ctx=ctx;return r;
}
static int readable(MuBtSelfReader*r,uint32_t a,size_t n){
 int count=0;
 if(devctl(r->fd,DCMD_PROC_MAPINFO,r->map,sizeof r->map,&count)!=EOK||
    count<1||count>=MAP_CAP)return 0;
 /* Reject crossing regions, even if adjacent. A mapping is not proof of
  * object type, ownership or lifetime. Reject explicit physical and noncached mappings. */
 for(int i=0;i<count;i++){
  const procfs_mapinfo*m=&r->map[i];uint64_t start=m->vaddr,len=m->size;
  if((m->flags&PROT_READ)&&!(m->flags&PROT_NOCACHE)&&!(m->flags&MAP_PHYS)&&
     (uint64_t)a>=start&&(uint64_t)a-start<len&&
     (uint64_t)n<=len-((uint64_t)a-start))return 1;
 }
 return 0;
}
int mu_bt_self_reader_read(void*ctx,uint32_t a,void*out,size_t n){
 MuBtSelfReader*r=ctx;
 if(!r||!out||!a||!n||n>READ_CAP||(uint64_t)a+n>UINT64_C(0x100000000)||
    r->pid!=getpid()||pthread_mutex_trylock(&r->lock)!=0)return 0;
 int ok=0;off_t pos=(off_t)a;
 if(pos>=0&&(uint64_t)pos==a&&r->allow(r->ctx,a,n)==1&&readable(r,a,n)&&
    lseek(r->fd,pos,SEEK_SET)==pos&&read(r->fd,r->temp,n)==(ssize_t)n&&
    r->allow(r->ctx,a,n)==1&&readable(r,a,n)){
  memcpy(out,r->temp,n);ok=1;
 }
 memset(r->temp,0,n);pthread_mutex_unlock(&r->lock);return ok;
}
void mu_bt_self_reader_close(MuBtSelfReader*r){
 if(!r)return;
 close(r->fd);pthread_mutex_destroy(&r->lock);free(r);
}
#else
MuBtSelfReader*mu_bt_self_reader_open(int(*allow)(void*,uint32_t,size_t),void*ctx){(void)allow;(void)ctx;return NULL;}
int mu_bt_self_reader_read(void*ctx,uint32_t a,void*out,size_t n){(void)ctx;(void)a;(void)out;(void)n;return 0;}
void mu_bt_self_reader_close(MuBtSelfReader*r){(void)r;}
#endif
