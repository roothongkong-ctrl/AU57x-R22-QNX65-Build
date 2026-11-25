/* SPDX-License-Identifier: GPL-3.0-only */
#include "inner_socket.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
static MuInnerSocket s;
static int good=1,events,busy;
static const unsigned char det[]={255,85,2,0,238,16};
static int proof(void *ctx,int fd,uint64_t e){return ctx==&s&&fd>=0&&good&&e==9;}
static int send_event(void *ctx,const unsigned char *p,size_t n){
 assert(ctx==&s&&n==sizeof det&&!memcmp(p,det,n));
 if(busy)return 0;
 events++;return 1;
}
int main(void){
 int f[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,f)==0);
 for(int i=0;i<2;i++)assert(fcntl(f[i],F_SETFL,O_NONBLOCK)==0);
 good=0;assert(mu_inner_socket_adopt(&s,f[0],9,&s,proof,send_event)==-1);
 good=1;assert(mu_inner_socket_adopt(&s,f[0],9,&s,proof,send_event)==1);
 assert(write(f[1],det,2)==2);
 assert(mu_inner_socket_tick(&s,9)==1&&events==0);
 assert(write(f[1],det+2,4)==4);
 assert(mu_inner_socket_tick(&s,9)==1&&events==0);
 busy=1;assert(mu_inner_socket_tick(&s,9)==1&&events==0);
 busy=0;assert(mu_inner_socket_tick(&s,9)==1&&events==1);
 assert(mu_inner_from_control(&s.relay,9,det,sizeof det)==1);
 assert(mu_inner_socket_tick(&s,9)==1);
 unsigned char out[8];assert(read(f[1],out,sizeof out)==6&&!memcmp(out,det,6));
 assert(mu_inner_socket_tick(&s,8)==-1&&s.relay.active);
 close(f[1]);assert(mu_inner_from_control(&s.relay,9,det,6)==1);
 assert(mu_inner_socket_tick(&s,9)==-1&&!s.relay.active&&s.fd==-1);
 assert(fcntl(f[0],F_GETFD)==-1&&errno==EBADF);
 puts("MU1005_INNER_UNIX_STREAM_HOST_PASS_NO_OEM");return 0;
}
