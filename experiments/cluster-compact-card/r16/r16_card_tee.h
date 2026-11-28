/*
 * R16 experimental bridge: decoded-for-transport AVCC access units (from the
 * real R13 112 AirPlay AES receiver) -> existing alt111 AVCC validator +
 * SPS/PPS/IDR priming -> local-only Annex-B TCP consumer at 127.0.0.1:19822.
 *
 * Serial ownership: ONE R13 112 receiver thread owns all calls to this
 * object and its alt111_video. No cross-thread access.
 * Safety: bounded nonblocking sends, backpressure disconnect, no disk writes.
 * This is an experimental data bridge, NOT a real CarPlay vehicle install.
 */
#ifndef CLUSTER_CARD_R16_112_LOCAL_TEE_H
#define CLUSTER_CARD_R16_112_LOCAL_TEE_H
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "alt111.h"
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#define R16_CARD_PORT 19822
#define R16_PUMP_BYTES_PER_TICK (1024u*1024u)
struct r16_card_tee {
    int listen_fd, client_fd;
    struct alt111_video *core;
    unsigned connected, detached, sends, would_block, errors;
    uint64_t sent_bytes;
};
static void r16_card_tee_reset(struct r16_card_tee *t)
{
    memset(t,0,sizeof(*t));
    t->listen_fd=-1;t->client_fd=-1;
}
static int r16_nonblock(int fd)
{
    int flags=fcntl(fd,F_GETFL,0);
    return flags>=0 && fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;
}
static void r16_disconnect(struct r16_card_tee*t)
{
    if(t->client_fd>=0){shutdown(t->client_fd,SHUT_RDWR);close(t->client_fd);}
    t->client_fd=-1;
    if(t->core && t->core->attached)alt111_video_detach(t->core);
    ++t->detached;
}
static int r16_card_tee_open(struct r16_card_tee*t,struct alt111_video *core,int port)
{
    struct sockaddr_in addr;
    int fd,one=1;
    if(!t||!core||!core->active||port<1024||port>65535)return -1;
    r16_card_tee_reset(t);
    t->core=core;
    fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)return -1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    addr.sin_port=htons((uint16_t)port);
    if(bind(fd,(const struct sockaddr*)&addr,sizeof(addr))<0 ||
       listen(fd,1)<0 || !r16_nonblock(fd)){
        close(fd);return -1;
    }
    t->listen_fd=fd;
    return 0;
}
static void r16_card_tee_close(struct r16_card_tee*t)
{
    if(!t)return;
    r16_disconnect(t);
    if(t->listen_fd>=0)close(t->listen_fd);
    t->listen_fd=-1;t->core=NULL;
}
static void r16_card_tee_pump(struct r16_card_tee*t)
{
    const uint8_t *bytes;
    size_t n,sent_budget=0;
    struct alt111_output_ticket ticket;
    int rc;
    if(!t||!t->core||t->listen_fd<0)return;
    if(t->client_fd>=0 && !t->core->attached)r16_disconnect(t);
    if(t->client_fd<0){
        int fd=accept(t->listen_fd,NULL,NULL);
        if(fd>=0){
            if(!r16_nonblock(fd)||alt111_video_attach(t->core)!=ALT111_OK){
                close(fd);++t->errors;return;
            }
            t->client_fd=fd;++t->connected;
        }else if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){
            ++t->errors;
        }
    }
    if(t->client_fd<0)return;
    while(sent_budget<R16_PUMP_BYTES_PER_TICK){
        ssize_t w;
        rc=alt111_video_peek(t->core,&bytes,&n,&ticket);
        if(rc!=ALT111_OK||!n)break;
        if(n>65536u)n=65536u;
        if(n>R16_PUMP_BYTES_PER_TICK-sent_budget)
            n=R16_PUMP_BYTES_PER_TICK-sent_budget;
        do{w=send(t->client_fd,bytes,n,MSG_DONTWAIT|MSG_NOSIGNAL);}
        while(w<0 && errno==EINTR);
        if(w>0){
            if(alt111_video_advance(t->core,&ticket,(size_t)w)!=ALT111_OK){
                ++t->errors;r16_disconnect(t);break;
            }
            sent_budget+=(size_t)w;t->sent_bytes+=(size_t)w;++t->sends;
            continue;
        }
        if(w<0 && (errno==EAGAIN||errno==EWOULDBLOCK)){
            ++t->would_block;break;
        }
        ++t->errors;r16_disconnect(t);break;
    }
}
#endif
