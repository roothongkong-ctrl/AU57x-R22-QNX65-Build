/* SPDX-License-Identifier: GPL-3.0-only
 * HOST fixture only. SDK callbacks and owner verification are test doubles.
 * No MFi, no original process, no vehicle execution.
 */
#define _POSIX_C_SOURCE 200809L
#include "daemon_client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
static const uint8_t peer[6]={1,2,3,4,5,6};
static const uint8_t marker[6]={0xff,0x55,2,0,0xee,0x10};
static struct {int allowed;uint8_t bytes[128];size_t n;} fixture={1,{0},0};
static int admission(void *ctx,uint32_t g,const uint8_t p[6]){
    (void)ctx;return fixture.allowed&&g==1&&!memcmp(peer,p,6);
}
static int daemon(void *ctx,int fd){
    (void)ctx;struct sockaddr_in a;socklen_t n=sizeof a;
    return getpeername(fd,(struct sockaddr*)&a,&n)==0&&
        a.sin_family==AF_INET&&a.sin_addr.s_addr==htonl(INADDR_LOOPBACK)&&
        a.sin_port==htons(57342);
}
static int sdk(void *ctx,const uint8_t *p,size_t n){
    (void)ctx;assert(n<=3&&fixture.n+n<=sizeof fixture.bytes);
    memcpy(fixture.bytes+fixture.n,p,n);fixture.n+=n;return 0;
}
static uint64_t now(void){
    struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
int main(void){
    AuRfIngress rf;AuDaemonClient c;
    AuDaemonAdmission g={0,admission,daemon};
    au_rf_init(&rf,sdk,0);assert(!au_rf_connected(&rf,peer));
    assert(!au_rf_set_max_payload(&rf,3));au_daemon_client_init(&c,&rf,g);
    fixture.allowed=0;assert(au_daemon_client_start(&c,now())<0);
    assert(c.fd<0&&rf.failed);
    au_rf_init(&rf,sdk,0);assert(!au_rf_connected(&rf,peer));
    assert(!au_rf_set_max_payload(&rf,3));au_daemon_client_init(&c,&rf,g);
    fixture.allowed=1;assert(!au_daemon_client_start(&c,now()));
    uint64_t start=now();int replied=0;
    while(now()-start<5000){
        assert(au_daemon_client_step(&c,now())>=0);
        if(replied&&fixture.n>6&&rf.tx_busy){
            assert(fixture.bytes[6]==0xff&&fixture.bytes[7]==0x5a);
            fixture.allowed=0;
            assert(au_daemon_client_step(&c,now())<0);
            assert(c.fd<0&&rf.tx_busy);
            assert(au_daemon_client_complete(&c,1)<0);
            assert(!rf.tx_busy&&rf.failed);
            puts("AU57X_DIRECT_DAEMON_MARKER_SYN_WIRED_REVOKE_OK_NO_OEM");return 0;
        }
        if(rf.tx_busy)assert(!au_daemon_client_complete(&c,1));
        if(!replied&&fixture.n==6&&!rf.tx_busy){
            assert(!memcmp(fixture.bytes,marker,6));
            assert(!au_rf_received(&rf,marker,2));
            assert(!au_rf_received(&rf,marker+2,4));replied=1;
        }
        struct timespec delay={0,1000000};nanosleep(&delay,0);
    }
    au_daemon_client_stop(&c);
    return 2;
}
