/* SPDX-License-Identifier: GPL-3.0-only
 * Exact R14 TCP bytes <-> R44 iAP2, using a real nonblocking socket.
 * Single-consumer loopback listener, no OEM process injection or implicit ownership claims.
 */
#include "cp_step04_socket_io.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int retryable(int err){
 return err==EAGAIN||err==EWOULDBLOCK||err==EINTR;
}
int cp_s4_listener_open(void){
 struct sockaddr_in a;
 int fd=socket(AF_INET,SOCK_STREAM,0),flags,one=1;
 if(fd<0)return -1;
 flags=fcntl(fd,F_GETFL,0);
 if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0||
    fcntl(fd,F_SETFD,FD_CLOEXEC)<0||
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one)<0)goto bad;
 memset(&a,0,sizeof a);a.sin_family=AF_INET;
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 a.sin_port=htons(CP_S4_LOCAL_PORT);
 if(bind(fd,(struct sockaddr*)&a,sizeof a)<0||listen(fd,1)<0)goto bad;
 return fd;
bad:close(fd);return -1;
}
int cp_s4_listener_accept(int listener,struct cp_s4_socket_io *w,
 struct cp_s4_tcp_bridge *t,int (*verified)(void*,int,uint32_t),void *opaque){
 struct sockaddr_in a; socklen_t size=sizeof a;
 int fd,r,flags;
 if(listener<0||!w||w->attached||!t||t->closed||t->connected||
    !t->epoch||!verified)return CP_LK_BAD;
 flags=fcntl(listener,F_GETFL,0);
 if(flags<0||!(flags&O_NONBLOCK))return CP_LK_BAD;
 fd=accept(listener,(struct sockaddr*)&a,&size);
 if(fd<0)return retryable(errno)?CP_LK_BACKPRESSURE:CP_LK_BAD;
 /* Reject unrelated connections before attach can revoke the idle session. */
 if(size!=sizeof a||a.sin_family!=AF_INET||
    a.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
    fcntl(fd,F_SETFD,FD_CLOEXEC)<0||verified(opaque,fd,t->epoch)!=1){
  close(fd);return CP_LK_OWNER;
 }
 r=cp_s4_socket_attach(w,t,fd,verified,opaque);
 if(r!=CP_LK_OK)close(fd);
 return r;
}
void cp_s4_socket_abort(struct cp_s4_socket_io *w){
 struct cp_s4_tcp_bridge *t;
 if(!w||!w->attached)return;
 t=w->bridge;
 (void)shutdown(w->fd,SHUT_RDWR);
 (void)close(w->fd);
 memset(w,0,sizeof *w);
 if(t)cp_s4_tcp_eof(t);
}
int cp_s4_socket_attach(struct cp_s4_socket_io *w,struct cp_s4_tcp_bridge *t,
                        int fd,int (*verified)(void *,int,uint32_t),void *opaque){
 struct sockaddr_in local,remote;
 socklen_t size;
 int flags;
 if(!w||w->attached||!t||!t->epoch||t->closed||!verified||fd<0)
  return CP_LK_BAD;
 size=sizeof local;
 if(getsockname(fd,(struct sockaddr *)&local,&size)!=0||
    size!=sizeof local||local.sin_family!=AF_INET||
    local.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
    local.sin_port!=htons(CP_S4_LOCAL_PORT))goto reject;
 size=sizeof remote;
 if(getpeername(fd,(struct sockaddr *)&remote,&size)!=0||
    size!=sizeof remote||remote.sin_family!=AF_INET||
    remote.sin_addr.s_addr!=htonl(INADDR_LOOPBACK))goto reject;
 /* OEM owner/process/session proof is deliberately a mandatory external gate. */
 if(verified(opaque,fd,t->epoch)!=1)goto reject;
#if !defined(MSG_NOSIGNAL) && !defined(SO_NOSIGPIPE)
 goto reject; /* no safe send without SIGPIPE suppression */
#elif !defined(MSG_NOSIGNAL)
 {
  int one=1;
  if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one)!=0)goto reject;
 }
#endif
 flags=fcntl(fd,F_GETFL,0);
 if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)goto reject;
 memset(w,0,sizeof *w);
 w->bridge=t;w->fd=fd;w->peer_verified=verified;
 w->opaque=opaque;w->attached=1;
 return CP_LK_OK;
reject:
 cp_s4_tcp_eof(t);
 return CP_LK_OWNER; /* rejected fd still belongs to caller */
}
int cp_s4_socket_poll(struct cp_s4_socket_io *w,uint64_t now){
 uint8_t rx[AU_RF_WIRE_MAX];
 const uint8_t *tx=NULL;
 size_t len=0;
 ssize_t n;
 int result;
 if(!w||!w->attached||!w->bridge)return CP_LK_BAD;
 if(!w->peer_verified||w->peer_verified(w->opaque,w->fd,
                                       w->bridge->epoch)!=1){
  cp_s4_socket_abort(w);return CP_LK_OWNER;
 }
 n=recv(w->fd,rx,sizeof rx,0);
 if(n==0){cp_s4_socket_abort(w);return CP_LK_DEAD_LINK;}
 if(n>0){
  result=cp_s4_tcp_feed(w->bridge,rx,(size_t)n,now);
  if(result!=CP_LK_OK){cp_s4_socket_abort(w);return result;}
 }else if(!retryable(errno)){
  cp_s4_socket_abort(w);return CP_LK_DEAD_LINK;
 }
 if(w->bridge->closed){cp_s4_socket_abort(w);return CP_LK_DEAD_LINK;}
 /* A newly accepted socket has no RFCOMM generation until CONNECT arrives.
  * Ticking the idle link here would incorrectly revoke a valid peer. */
 if(!w->bridge->connected)return CP_LK_OK;
 result=cp_s4_tcp_tick(w->bridge,now);
 if(result!=CP_LK_OK&&result!=CP_LK_BACKPRESSURE){
  cp_s4_socket_abort(w);return result;
 }
 /* No output can be consumed until real CONNECT has established generation. */
 if(!w->bridge->connected)return CP_LK_OK;
 result=cp_s4_tcp_peek(w->bridge,&tx,&len);
 if(result<0){cp_s4_socket_abort(w);return result;}
 if(!result)return CP_LK_OK;
 if(!tx||!len){cp_s4_socket_abort(w);return CP_LK_BAD;}
#ifdef MSG_NOSIGNAL
 n=send(w->fd,tx,len,MSG_NOSIGNAL);
#else
 n=send(w->fd,tx,len,0);
#endif
 if(n>0){
  result=cp_s4_tcp_commit(w->bridge,(size_t)n);
  if(result!=CP_LK_OK){cp_s4_socket_abort(w);return result;}
  return CP_LK_OK;
 }
 if(n<0&&retryable(errno))return CP_LK_OK;
 cp_s4_socket_abort(w);
 return CP_LK_DEAD_LINK;
}
