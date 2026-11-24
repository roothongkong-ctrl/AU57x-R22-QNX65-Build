#define _POSIX_C_SOURCE 200809L
#ifdef __QNXNTO__
#define _QNX_SOURCE 1
#endif
#include "r123_rf_transport.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
static int64_t now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int bad(R123Transport*t){if(t&&t->opened&&!t->failed){t->failed=1;t->active=0;t->used=t->target=t->offset=0;memset(t->wire,0,sizeof t->wire);shutdown(t->fd,SHUT_RDWR);}return -1;}
void r123_transport_revoke(R123Transport*t){if(t)bad(t);}
static int waitfd(R123Transport*t,short event,int64_t end){
 for(;;){int64_t n=now();if(n<0)return -1;int64_t left=end-n;struct pollfd p={t->fd,event,0};int rc=poll(&p,1,left>0?(int)left:0);if(rc<0&&errno==EINTR){if(left<=0)return 0;continue;}if(rc<=0)return rc;if(p.revents&event)return 1;return -1;}
}
int r123_transport_open(R123Transport*t,int fd,const uint8_t peer[6],uint32_t gen){
 struct sockaddr_in local,remote;socklen_t a=sizeof local,b=sizeof remote;uint8_t temp[14];size_t n;
 if(!t||t->opened||fd<0||!peer||wc_wire_encode(WC_RF_CONNECTED,gen,peer,6,temp,sizeof temp,&n)||getsockname(fd,(struct sockaddr*)&local,&a)||getpeername(fd,(struct sockaddr*)&remote,&b)||a!=sizeof local||b!=sizeof remote||local.sin_family!=AF_INET||remote.sin_family!=AF_INET||local.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||remote.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||local.sin_port!=htons(57342))return -1;
#if !defined(MSG_NOSIGNAL) && !defined(SO_NOSIGPIPE)
 return -1;
#elif !defined(MSG_NOSIGNAL)
 int yes=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof yes))return -1;
#endif
 int fl=fcntl(fd,F_GETFL,0);if(fl<0||fcntl(fd,F_SETFL,fl|O_NONBLOCK)<0)return -1;
 memset(t,0,sizeof(*t));t->fd=fd;t->opened=1;t->generation=gen;memcpy(t->peer,peer,6);return 0;
}
/* Read exactly one frame, retaining a partial header/payload across calls. */
static int read_frame(R123Transport*t,int64_t end){
 while(!t->target||t->used<t->target){
  size_t need=(t->target?t->target:WC_RF_HEADER)-t->used;
  int rc=waitfd(t,POLLIN,end);if(rc==0)return 0;if(rc<0)return bad(t);
  ssize_t n=recv(t->fd,t->wire+t->used,need,0);
  if(n<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK)){if(now()>=end)return 0;continue;}
  if(n<=0)return bad(t);
  t->used+=(size_t)n;
  if(!t->target&&t->used==WC_RF_HEADER){size_t len=(size_t)t->wire[2]|((size_t)t->wire[3]<<8);if(t->wire[1]||len>WC_RF_MAX)return bad(t);t->target=WC_RF_HEADER+len;}
 }
 return 1;
}
static int await_peer(R123Transport*t,int64_t end){
 if(t->active)return 1;
 int rc=read_frame(t,end);if(rc<=0)return rc;
 WcWireFrame f;if(wc_wire_decode(t->wire,t->used,&f)||f.generation!=t->generation||f.kind!=WC_RF_CONNECTED||memcmp(f.data,t->peer,6))return bad(t);
 t->active=1;t->target=t->used=t->offset=0;return 1;
}
int r123_transport_wait_connected(R123Transport*t,int ms){
 if(!t||!t->opened||t->failed||ms<0||ms>60000)return -1;
 int64_t end=now();if(end<0)return bad(t);return await_peer(t,end+ms);
}
/* Nonconsuming readiness for OEM input notification. Never deliver an
 * input-ready event for CONNECTED, partial frames, or disconnected peers. */
int r123_transport_data_ready(R123Transport*t){
 if(!t||!t->opened||t->failed||!t->active)return -1;
 int64_t end=now();if(end<0)return bad(t);
 int rc=read_frame(t,end);if(rc<=0)return rc;
 WcWireFrame f;if(wc_wire_decode(t->wire,t->used,&f)||f.generation!=t->generation||f.kind!=WC_RF_DATA)return bad(t);
 return 1;
}
int r123_transport_recv(R123Transport*t,void*data,unsigned cap,int ms){
 if(!t||!t->opened||t->failed||!data||!cap||ms<0||ms>60000)return -1;
 int64_t end=now();if(end<0)return bad(t);end+=ms;
 int rc=await_peer(t,end);if(rc<=0)return rc;
 rc=read_frame(t,end);if(rc<=0)return rc;
 WcWireFrame f;if(wc_wire_decode(t->wire,t->used,&f)||f.generation!=t->generation||f.kind!=WC_RF_DATA)return bad(t);
 size_t remain=f.size-t->offset,k=cap<remain?cap:remain;memcpy(data,f.data+t->offset,k);t->offset+=k;
 if(t->offset==f.size)t->offset=t->target=t->used=0;
 return (int)k;
}
int r123_transport_send(R123Transport*t,const void*data,unsigned n,int ms){
 if(!t||!t->opened||!t->active||t->failed||!data||!n||n>WC_RF_MAX||ms<0||ms>60000)return -1;
 uint8_t wire[WC_RF_HEADER+WC_RF_MAX];size_t size;if(wc_wire_encode(WC_RF_DATA,t->generation,data,n,wire,sizeof wire,&size))return bad(t);
 int64_t end=now();if(end<0)return bad(t);end+=ms;size_t sent=0;
 while(sent<size){int rc=waitfd(t,POLLOUT,end);if(!rc)return sent?bad(t):0;if(rc<0)return bad(t);
#ifdef MSG_NOSIGNAL
  int flags=MSG_NOSIGNAL;
#else
  int flags=0;
#endif
  ssize_t k=send(t->fd,wire+sent,size-sent,flags);
  if(k<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK)){if(now()>=end)return sent?bad(t):0;continue;}if(k<=0)return bad(t);sent+=(size_t)k;
 }
 return (int)n;
}
