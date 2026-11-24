/* SPDX-License-Identifier: GPL-3.0-only */
#include "daemon_client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define AU_DAEMON_PORT 57342u
static int peer_ok(const uint8_t p[6]){
    unsigned any=0,all=255;
    for(unsigned i=0;i<6;i++){any|=p[i];all&=p[i];}
    return any && all!=255;
}
void au_daemon_client_init(AuDaemonClient *c,AuRfIngress *rf,AuDaemonAdmission g){
    memset(c,0,sizeof *c);c->fd=-1;c->rf=rf;c->guard=g;
    au_ipc_init(&c->stream,rf);
}
static int live(AuDaemonClient *c){
    return c&&c->rf&&c->guard.admit&&c->guard.daemon_verified&&
        !c->rf->failed&&c->rf->connected&&c->generation&&
        c->rf->generation==c->generation&&peer_ok(c->peer)&&
        !memcmp(c->peer,c->rf->peer,6)&&
        c->guard.admit(c->guard.ctx,c->generation,c->peer)==1;
}
void au_daemon_client_stop(AuDaemonClient *c){
    if(!c)return;
    if(c->fd>=0){close(c->fd);c->fd=-1;}
    /* R45 preserves asynchronous SDK-owned bytes until actual completion. */
    if(c->generation)au_ipc_socket_lost(&c->stream);
    c->connecting=c->active=0;c->generation=0;
}
static int fail(AuDaemonClient *c){au_daemon_client_stop(c);return -1;}
int au_daemon_client_start(AuDaemonClient *c,uint64_t now){
    if(!c||!c->rf||c->fd>=0||c->stream.failed||c->generation)return -1;
    c->generation=c->rf->generation;memcpy(c->peer,c->rf->peer,6);
    if(!live(c))return fail(c);
    c->fd=socket(AF_INET,SOCK_STREAM,0);
    if(c->fd<0)return fail(c);
    int flags=fcntl(c->fd,F_GETFL,0);
    if(flags<0||fcntl(c->fd,F_SETFL,flags|O_NONBLOCK)<0||
       fcntl(c->fd,F_SETFD,FD_CLOEXEC)<0)return fail(c);
#if !defined(MSG_NOSIGNAL)
# if defined(SO_NOSIGPIPE)
    int one=1;
    if(setsockopt(c->fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one)<0)return fail(c);
# else
    /* Never alter process-wide SIGPIPE handling inside OEM btstack. */
    return fail(c);
# endif
#endif
    struct sockaddr_in address;
    memset(&address,0,sizeof address);
    address.sin_family=AF_INET;
    address.sin_port=htons(AU_DAEMON_PORT);
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    c->started_ms=now;
    int result=connect(c->fd,(struct sockaddr*)&address,sizeof address);
    if(result==0)c->connecting=1;
    else if(errno==EINPROGRESS||errno==EWOULDBLOCK)c->connecting=1;
    else return fail(c);
    return 0;
}
int au_daemon_client_step(AuDaemonClient *c,uint64_t now){
    if(!c||c->fd<0)return -1;
    if(!live(c))return fail(c);
    if(c->connecting){
        if(now<c->started_ms||now-c->started_ms>2000)return fail(c);
        struct pollfd p={c->fd,POLLOUT,0};
        int n=poll(&p,1,0);
        if(n<0&&errno==EINTR)return 0;
        if(n<0)return fail(c);
        if(!n)return 0;
        int error=0;socklen_t length=sizeof error;
        if(p.revents&(POLLERR|POLLHUP|POLLNVAL)||
           getsockopt(c->fd,SOL_SOCKET,SO_ERROR,&error,&length)<0||error)
            return fail(c);
        if(c->guard.daemon_verified(c->guard.ctx,c->fd)!=1)return fail(c);
        c->connecting=0;c->active=1;
    }
    if(!c->active||c->guard.daemon_verified(c->guard.ctx,c->fd)!=1)return fail(c);
    if(au_socket_step(&c->stream,c->fd,0)<0)return fail(c);
    return 1;
}
int au_daemon_client_complete(AuDaemonClient *c,int success){
    if(!c||!c->rf||!c->rf->tx_busy)return -1;
    /* Even revoked sessions must release the genuine outstanding SDK buffer. */
    if(c->fd<0||!live(c)||!c->active||
       c->guard.daemon_verified(c->guard.ctx,c->fd)!=1){
        au_daemon_client_stop(c);
        (void)au_rf_send_complete(c->rf,success);
        return -1;
    }
    if(au_socket_tx_complete(&c->stream,success)<0)return fail(c);
    return 0;
}
