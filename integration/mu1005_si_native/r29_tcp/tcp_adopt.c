/* GPL-3.0-or-later; explicit review-only adapter for R98 loopback TCP. */
#include "tcp_adopt.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
int wcp_r29_tcp_adopt(struct wcp_oem_graft_io *s,int fd,unsigned listen_port,wcp_peer_proof proof,void *ctx){
 struct sockaddr_in peer,local;
 socklen_t pn=sizeof peer,ln=sizeof local,tn=sizeof(int);
 int type,flags;
 if(!s||fd<0||!proof||!listen_port||listen_port>65535)return -EINVAL;
#if !defined(WCP_TCP_HOST_TEST)
 if(listen_port!=57342)return -EINVAL;
#endif
 if(getpeername(fd,(struct sockaddr*)&peer,&pn)<0||getsockname(fd,(struct sockaddr*)&local,&ln)<0||
    pn!=sizeof peer||ln!=sizeof local||peer.sin_family!=AF_INET||local.sin_family!=AF_INET||
    peer.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||local.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
    ntohs(local.sin_port)!=listen_port||!peer.sin_port||
    getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&tn)<0||tn!=sizeof type||type!=SOCK_STREAM)return -EINVAL;
 flags=fcntl(fd,F_GETFL,0);
 if(flags<0||!(flags&O_NONBLOCK))return -EINVAL;
 if(proof(ctx,fd,listen_port,ntohs(peer.sin_port))!=1)return -EACCES;
 if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0)return -errno;
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
 {int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one)<0)return -errno;}
#endif
 memset(s,0,sizeof *s);s->fd=fd;
 return wcp_graft_endpoint_init(&s->endpoint);
}
