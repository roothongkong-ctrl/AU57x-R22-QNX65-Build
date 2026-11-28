/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "route3_fd.h"
#include "byte_contract.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
uint64_t route3_fd_now(void*unused){
 struct timespec t;(void)unused;
 if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT64_MAX;
 return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
}
static int alive(void*v,const uint8_t*m,uint32_t g){
 Route3Fd*s=v;return s->attached&&s->proof(s->ctx,s->fd,m,g)==1;
}
static int emit(void*v,const uint8_t*b,size_t n){
 Route3Fd*s=v;
 if(!s->attached||s->used||!n||n>sizeof s->tx||
    !alive(s,s->link.peer,s->link.radio_generation))return -1;
 memcpy(s->tx,b,n);s->used=n;s->offset=0;return 0;
}
static int fail(Route3Fd*s){
 cp_step2_ipc_lost(&s->link);s->used=s->offset=0;s->attached=0;
 return -1; /* owner closes the borrowed fd; never retry an uncertain send */
}
int route3_fd_attach(Route3Fd*s,int fd,void*ctx,int(*proof)(void*,int,const uint8_t*,uint32_t)){
 struct sockaddr_storage peer;socklen_t len=sizeof peer;int type; socklen_t tl=sizeof type;
 if(!s||s->attached||fd<0||!proof||proof(ctx,fd,NULL,0)!=1||
    getpeername(fd,(struct sockaddr*)&peer,&len)||getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&tl)||
    type!=SOCK_STREAM)return -1;
 int flags=fcntl(fd,F_GETFL,0);if(flags<0||!(flags&O_NONBLOCK))return -1;
#if !defined(MSG_NOSIGNAL) && !defined(SO_NOSIGPIPE)
 return -1;
#elif !defined(MSG_NOSIGNAL)
 int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one))return -1;
#endif
 memset(s,0,sizeof *s);s->fd=fd;s->ctx=ctx;s->proof=proof;s->attached=1;
 struct cp_step2_ipc_owner o={s,alive,alive,emit};cp_step2_ipc_init(&s->link,&o);return 0;
}
static int retry(int e){return e==EINTR||e==EAGAIN||e==EWOULDBLOCK;}
int route3_fd_step(Route3Fd*s,uint64_t deadline){
 if(!s||!s->attached)return -1;
 if(s->proof(s->ctx,s->fd,NULL,0)!=1||
    (s->link.connected&&!alive(s,s->link.peer,s->link.radio_generation)))return fail(s);
 for(;;){
  uint64_t now=route3_fd_now(NULL);if(now>=deadline)return 0;
  uint64_t wait=deadline-now;
  struct pollfd p={s->fd,(short)(POLLIN|(s->used?POLLOUT:0)),0};
  int r=poll(&p,1,wait>INT_MAX?INT_MAX:(int)wait);
  if(r<0){if(errno==EINTR)continue;return fail(s);}
  if(!r)return 0;
  if(s->proof(s->ctx,s->fd,NULL,0)!=1||
     (s->link.connected&&!alive(s,s->link.peer,s->link.radio_generation)))return fail(s);
  if(p.revents&(POLLERR|POLLNVAL))return fail(s);
  int progress=0;
  if((p.revents&POLLOUT)&&s->used){
#ifdef MSG_NOSIGNAL
   ssize_t n=send(s->fd,s->tx+s->offset,s->used-s->offset,MSG_NOSIGNAL);
#else
   ssize_t n=send(s->fd,s->tx+s->offset,s->used-s->offset,0);
#endif
   if(n>0){s->offset+=(size_t)n;progress=1;if(s->offset==s->used)s->offset=s->used=0;}
   else if(n==0||!retry(errno))return fail(s);
  }
  if(p.revents&(POLLIN|POLLHUP)){
   uint8_t b[CP_STEP2_IPC_MAX];ssize_t n=recv(s->fd,b,sizeof b,0);
   if(n>0){
    unsigned long before=s->link.tx_confirmed;
    if(cp_step2_ipc_feed(&s->link,b,(size_t)n)!=CP_BT_OK||
       (s->used&&s->link.tx_confirmed!=before))return fail(s);
    if(s->link.finished){s->attached=0;s->used=s->offset=0;return -1;}
    progress=1;
   }else if(n==0||!retry(errno))return fail(s);
  }
  if(progress)return 1;
  if(p.revents&POLLHUP)return fail(s);
 }
}
static int step(void*v,struct cp_step2_ipc_link*l,uint64_t d){
 Route3Fd*s=v;return l==&s->link?route3_fd_step(s,d):-1;
}
int route3_fd_send(Route3Fd*s,const uint8_t*b,size_t n,uint64_t deadline){
 if(!s||!s->attached||s->used)return -1;
 Route3IpcFinish f={&s->link,s,step,route3_fd_now,deadline,4096};
 int rc=route3_ipc_send(&f,b,n);
 if(rc<0&&s->link.poisoned)return fail(s);
 return rc;
}
int route3_fd_recv(Route3Fd*s,uint8_t*b,size_t n){
 return s&&s->attached?route3_recv_now(&s->link.transport,b,n):-1;
}
void route3_fd_detach(Route3Fd*s){if(s&&s->attached)(void)fail(s);}
