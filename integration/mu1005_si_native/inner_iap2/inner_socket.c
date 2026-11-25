/* SPDX-License-Identifier: GPL-3.0-only */
#include "inner_socket.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
static int live(void *ctx,uint64_t e){
 MuInnerSocket *s=ctx;
 return s->fd>=0&&s->verified(s->ctx,s->fd,e)==1;
}
static int event(void *ctx,const unsigned char *p,size_t n){
 MuInnerSocket *s=ctx;return s->event(s->ctx,p,n);
}
static int write_peer(void *ctx,const unsigned char *p,size_t n){
 MuInnerSocket *s=ctx;
#ifdef MSG_NOSIGNAL
 int flags=MSG_NOSIGNAL;
#else
 int flags=0;
#endif
 ssize_t r=send(s->fd,p,n,flags);
 if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;
 return r>0?(int)r:-1;
}
void mu_inner_socket_close(MuInnerSocket *s){
 if(!s)return;
 if(s->verified&&s->fd>=0)close(s->fd);
 mu_inner_end(&s->relay);s->fd=-1;s->verified=NULL;s->event=NULL;s->ctx=NULL;
}
int mu_inner_socket_adopt(MuInnerSocket *s,int fd,uint64_t e,void *ctx,
 int (*proof)(void *,int,uint64_t),int (*send_event)(void *,const unsigned char *,size_t)){
 if(!s||s->verified||s->relay.active||fd<0||!e||!proof||!send_event)return -1;
 struct sockaddr_un a,b;
 socklen_t an=sizeof a,bn=sizeof b,tn=sizeof(int);
 int type=0,flags=fcntl(fd,F_GETFL,0);
 if(flags<0||!(flags&O_NONBLOCK)||getsockname(fd,(struct sockaddr*)&a,&an)<0||
    getpeername(fd,(struct sockaddr*)&b,&bn)<0||a.sun_family!=AF_UNIX||
    b.sun_family!=AF_UNIX||getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&tn)<0||
    type!=SOCK_STREAM||proof(ctx,fd,e)!=1)return -1;
#if !defined(MSG_NOSIGNAL)
#ifdef SO_NOSIGPIPE
 int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one)<0)return -1;
#else
 return -1; /* never expose OEM parent to SIGPIPE */
#endif
#endif
 if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0)return -1;
 s->fd=fd;s->ctx=ctx;s->verified=proof;s->event=send_event;
 MuInnerOps o={s,live,event,write_peer};
 if(mu_inner_start(&s->relay,e,&o)!=1){
  s->fd=-1;s->ctx=NULL;s->verified=NULL;s->event=NULL;return -1;
 }
 return 1;
}
int mu_inner_socket_tick(MuInnerSocket *s,uint64_t e){
 if(!s||!s->verified||s->fd<0||s->relay.epoch!=e)return -1;
 if(mu_inner_pump(&s->relay,e)!=1)goto lost;
 unsigned char buf[2048];
 size_t cap=MU_INNER_CAP-s->relay.tx_len;
 if(cap>sizeof buf)cap=sizeof buf;
 if(!cap)return 1;
 ssize_t n=recv(s->fd,buf,cap,0);
 if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 1;
 if(n<=0||mu_inner_from_cinemo(&s->relay,e,buf,(size_t)n)!=1)goto lost;
 return 1;
lost:mu_inner_socket_close(s);return -1;
}
