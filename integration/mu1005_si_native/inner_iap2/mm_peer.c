/* SPDX-License-Identifier: GPL-3.0-only */
#include "mm_peer.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
static MuMmPeer *bound;
static int fail(MuMmPeer *p){
 if(p&&p->active){close(p->fd);p->fd=-1;p->active=0;p->failed=1;
 p->tx_used=p->rx_used=0;}
 errno=EIO;return -1;
}
static int live(MuMmPeer *p){
 return p&&p==bound&&p->active&&p->proof(p->ctx,p->device,p->fd,p->epoch)==1;
}
int mu_mm_peer_adopt(MuMmPeer *p,void *device,int fd,uint64_t epoch,void *ctx,
 int(*proof)(void*,void*,int,uint64_t),int(*notify)(void*,void*)){
 if(!p||bound||p->active||!device||fd<0||!epoch||!proof||!notify)return -1;
 struct sockaddr_un local,peer;
 socklen_t ln=sizeof local,pn=sizeof peer,tn=sizeof(int);
 int type=0,flags=fcntl(fd,F_GETFL,0);
 if(flags<0||!(flags&O_NONBLOCK)||getsockname(fd,(struct sockaddr*)&local,&ln)<0||
    getpeername(fd,(struct sockaddr*)&peer,&pn)<0||local.sun_family!=AF_UNIX||
    peer.sun_family!=AF_UNIX||getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&tn)<0||
    type!=SOCK_STREAM||proof(ctx,device,fd,epoch)!=1)return -1;
#if !defined(MSG_NOSIGNAL)
#ifdef SO_NOSIGPIPE
 int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one)<0)return -1;
#else
 return -1;
#endif
#endif
 if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0)return -1;
 memset(p,0,sizeof *p);p->fd=fd;p->device=device;p->ctx=ctx;
 p->epoch=epoch;p->proof=proof;p->notify=notify;p->active=1;bound=p;return 1;
}
int mu_mm_peer_send(void *device,const void *bytes,unsigned n){
 MuMmPeer *p=bound;
 if(!p||device!=p->device||!bytes||!n||n>MU_INNER_CAP){errno=EINVAL;return -1;}
 if(!live(p))return fail(p);
 /* Native driver compares return value to original whole packet length.
  * Accept it atomically into a bounded queue; never report a short send.
  * Queue admission is not remote delivery or an iAP2 acknowledgement. */
 if(n>MU_INNER_CAP-p->tx_used){errno=EAGAIN;return -1;}
 memcpy(p->tx+p->tx_used,bytes,n);p->tx_used+=n;return (int)n;
}
int mu_mm_peer_recv(void *device,void *bytes,unsigned n,int ms,void *opaque){
 (void)opaque;MuMmPeer *p=bound;
 if(!p||device!=p->device||!bytes||!n||ms){errno=EINVAL;return -1;}
 if(!live(p))return fail(p);
 if(n>p->rx_used)n=(unsigned)p->rx_used;
 if(n){memcpy(bytes,p->rx,n);p->rx_used-=n;memmove(p->rx,p->rx+n,p->rx_used);}
 return (int)n;
}
static int again(void){return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR;}
int mu_mm_peer_tick(MuMmPeer *p){
 if(!p||p!=bound||p->ticking)return -1;
 if(!live(p))return fail(p);
 p->ticking=1;
 if(p->tx_used){
#ifdef MSG_NOSIGNAL
  int flags=MSG_NOSIGNAL;
#else
  int flags=0;
#endif
  ssize_t n=send(p->fd,p->tx,p->tx_used,flags);
  if(n<0&&!again())goto bad;
  if(n>0){p->tx_used-=(size_t)n;memmove(p->tx,p->tx+n,p->tx_used);}
 }
 if(p->rx_used<MU_INNER_CAP){
  ssize_t n=recv(p->fd,p->rx+p->rx_used,MU_INNER_CAP-p->rx_used,0);
  if(!n||(n<0&&!again()))goto bad;
  if(n>0)p->rx_used+=(size_t)n;
 }
 /* Actual notification adapter must be supplied by the owner.
  * The notification can synchronously invoke the native recv slot.
  * Post once per nonempty batch, recheck after callbacks. */
 if(p->rx_used&&p->notify(p->ctx,p->device)!=1)goto bad;
 if(!live(p))goto bad;
 p->ticking=0;return 1;
bad:p->ticking=0;return fail(p);
}
int mu_mm_peer_release(MuMmPeer *p){
 if(!p||p!=bound||p->ticking)return -1;
 if(p->active)close(p->fd);
 memset(p,0,sizeof *p);p->fd=-1;bound=NULL;return 0;
}
