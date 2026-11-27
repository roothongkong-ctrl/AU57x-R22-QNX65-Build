/* SPDX-License-Identifier: GPL-3.0-only
 * Actual MU1005 C transport -> adapted upstream Rust daemon with explicit ACK.
 * Synthetic RF peer; no OEM calls, MFi credentials or radio. */
#define _POSIX_C_SOURCE 200809L
#include "mu1005_socket_pump.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef struct{unsigned char bytes[8192];size_t used;unsigned chunks;} Capture;
static int sdk_send(void*p,const uint8_t*v,size_t n){
 Capture*c=p;
 if(!n||n>5||n>sizeof c->bytes-c->used)return -1;
 memcpy(c->bytes+c->used,v,n);c->used+=n;++c->chunks;return 0;
}
static void pause_ms(void){struct timespec t={0,50000000L};(void)nanosleep(&t,NULL);}
static int dial(void){
 struct sockaddr_in a={0};a.sin_family=AF_INET;
 a.sin_port=htons(57342);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 for(int i=0;i<100;i++){
  int fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd>=0&&connect(fd,(struct sockaddr*)&a,sizeof a)==0){
   int flags=fcntl(fd,F_GETFL,0);assert(flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0);
   return fd;
  }
  if(fd>=0)close(fd);
  pause_ms();
 }
 return -1;
}
static void step(AuIpcStream*s,int fd){
 assert(au_socket_step(s,fd,20)>=0);
 while(s->rf->tx_busy)assert(au_socket_tx_complete(s,1)>=0);
}
static int syn(const Capture*c){
 static const uint8_t marker[]={0xff,0x55,0x02,0,0xee,0x10};
 if(c->used<8||memcmp(c->bytes,marker,sizeof marker))return 0;
 for(size_t i=sizeof marker;i+1<c->used;i++)
  if(c->bytes[i]==0xff&&c->bytes[i+1]==0x5a)return 1;
 return 0;
}
static void run(void){
 const uint8_t addr[]={2,1,2,3,4,5};
 const uint8_t marker[]={0xff,0x55,0x02,0,0xee,0x10};
 AuRfIngress rf;AuIpcStream ipc;Capture cap={0};
 int fd=dial();assert(fd>=0);
 au_rf_init(&rf,sdk_send,&cap);assert(au_rf_enable_completion_ack(&rf)==0);au_ipc_init(&ipc,&rf);
 for(unsigned session=0;session<2;session++){
  uint64_t before=rf.tx_receipt.sequence;
  memset(&cap,0,sizeof cap);
  assert(au_rf_connected(&rf,addr)==0);
  assert(rf.generation==session*2+1);
  assert(au_rf_set_max_payload(&rf,5)==0);
  assert(au_rf_received(&rf,marker,2)==0);
  assert(au_rf_received(&rf,marker+2,4)==0);
  for(int i=0;i<300&&!syn(&cap);i++)step(&ipc,fd);
  assert(syn(&cap)&&cap.chunks>=2);
  assert(rf.tx_receipt.sequence>before && rf.tx_receipt.generation==rf.generation);
  for(int i=0;i<100&&(rf.count||ipc.out_pending);i++)step(&ipc,fd);
  assert(!rf.count&&!ipc.out_pending);
  printf("MU1005_C_DONOR_RUST generation=%u bytes=%zu fragments=%u marker_and_syn=PASS\n",rf.generation,cap.used,cap.chunks);
  assert(au_rf_disconnected(&rf)==0);
  /* Drain the DISCONNECTED frame before starting a new generation. */
  for(int i=0;i<100&&(rf.count||ipc.out_pending);i++)step(&ipc,fd);
  assert(!rf.count&&!ipc.out_pending);
 }
 close(fd);
}
int main(void){run();run();puts("MU1005_DONOR_ADAPTED_ACK_C_RUST_FOUR_GENERATIONS_PASS_NO_OEM");return 0;}
