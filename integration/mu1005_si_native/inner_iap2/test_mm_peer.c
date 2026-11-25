/* SPDX-License-Identifier: GPL-3.0-only */
#include "mm_peer.h"
#include "inner_socket.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
static MuMmPeer m;
static MuInnerSocket d;
static int device,allowed=1,events,notifies;
static unsigned char marker[]={255,85,2,0,238,16},received[6];
static size_t used;
static int mproof(void *ctx,void *dev,int fd,uint64_t e){
 return ctx==&m&&dev==&device&&fd>=0&&e==17&&allowed;
}
static int dproof(void *ctx,int fd,uint64_t e){return ctx==&d&&fd>=0&&e==17&&allowed;}
static int notice(void *ctx,void *dev){
 assert(ctx==&m&&dev==&device);notifies++;
 int n=mu_mm_peer_recv(dev,received+used,2,0,NULL);
 assert(n>0&&used+(size_t)n<=sizeof received);used+=(size_t)n;return 1;
}
static int event(void *ctx,const unsigned char *p,size_t n){
 assert(ctx==&d&&n==6&&!memcmp(p,marker,n));events++;return 1;
}
int main(void){
 int f[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,f)==0);
 for(int i=0;i<2;i++)assert(fcntl(f[i],F_SETFL,O_NONBLOCK)==0);
 assert(mu_mm_peer_adopt(&m,&device,f[0],17,&m,mproof,notice)==1);
 assert(mu_inner_socket_adopt(&d,f[1],17,&d,dproof,event)==1);
 assert(mu_mm_peer_send(&device,marker,6)==6);
 assert(mu_mm_peer_tick(&m)==1);
 assert(mu_inner_socket_tick(&d,17)==1);
 assert(mu_inner_socket_tick(&d,17)==1&&events==1);
 assert(mu_inner_from_control(&d.relay,17,marker,6)==1);
 assert(mu_inner_socket_tick(&d,17)==1);
 for(int i=0;i<3;i++)assert(mu_mm_peer_tick(&m)==1);
 assert(used==6&&notifies==3&&!memcmp(received,marker,6));
 assert(mu_mm_peer_recv(&device,received,6,0,NULL)==0);
 allowed=0;assert(mu_mm_peer_tick(&m)==-1&&m.failed);
 assert(mu_inner_socket_tick(&d,17)==-1);
 assert(mu_mm_peer_release(&m)==0);
 puts("MU1005_INNER_MM_PEER_TO_DIO_SOCKET_HOST_PASS_NO_OEM");return 0;
}
