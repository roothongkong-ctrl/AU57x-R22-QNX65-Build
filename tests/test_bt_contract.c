/* Host-only normalized Bluetooth fragmentation/reconnect regression; no OEM hooks. */
#include "mu1005_rf_ingress.h"
#include "mu1005_ipc_stream.h"
#include "mu1005_socket_pump.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static AuRfIngress rf;
static AuIpcStream ipc;
static int chunks;
static unsigned char accepted[8];
static size_t accepted_n;
static int sdk_send(void *unused,const uint8_t *bytes,size_t n){
 (void)unused; assert(n<=2 && n>0); assert(accepted_n+n<=sizeof accepted);
 memcpy(accepted+accepted_n,bytes,n);accepted_n+=n;chunks++;return 0;
}
static void put(uint8_t *p,unsigned type,unsigned n,uint32_t gen){
 p[0]=type;p[1]=0;p[2]=(uint8_t)n;p[3]=(uint8_t)(n>>8);
 for(unsigned i=0;i<4;i++)p[4+i]=(uint8_t)(gen>>(8*i));
}
int main(void){
 const uint8_t mac[6]={1,2,3,4,5,6};
 au_rf_init(&rf,sdk_send,0);au_ipc_init(&ipc,&rf);
 assert(au_socket_step(NULL,-1,0)==AU_SOCKET_IO_ERROR);
 assert(au_rf_connected(&rf,mac)==0 && rf.generation==1);
 const uint8_t *buf=0;size_t n=0;
 assert(au_ipc_peek(&ipc,&buf,&n)==1 && n==14 && buf[0]==1 && memcmp(buf+8,mac,6)==0);
 assert(au_ipc_commit(&ipc,3)==0);assert(au_ipc_peek(&ipc,&buf,&n)==1 && n==11);
 assert(au_ipc_commit(&ipc,11)==0);
 assert(au_rf_set_max_payload(&rf,2)==0);
 uint8_t pkt[13];put(pkt,2,5,1);memcpy(pkt+8,"hello",5);
 assert(au_ipc_feed(&ipc,pkt,3)==0 && chunks==0);
 assert(au_ipc_feed(&ipc,pkt+3,2)==0 && chunks==0);
 assert(au_ipc_feed(&ipc,pkt+5,8)==0 && chunks==1 && accepted_n==2);
 assert(au_socket_tx_complete(&ipc,1)==AU_SOCKET_PROGRESS && chunks==2);
 assert(au_socket_tx_complete(&ipc,1)==AU_SOCKET_PROGRESS && chunks==3);
 assert(au_socket_tx_complete(&ipc,1)==AU_SOCKET_PROGRESS && !rf.tx_busy);
 assert(accepted_n==5 && !memcmp(accepted,"hello",5));
 assert(au_rf_received(&rf,"ABC",3)==0);
 assert(au_ipc_peek(&ipc,&buf,&n)==1 && n==11 && buf[0]==2);
 assert(au_ipc_commit(&ipc,n)==0 && ipc.rx_frames_committed==1);
 assert(au_rf_disconnected(&rf)==0 && rf.generation==2);
 assert(au_ipc_peek(&ipc,&buf,&n)==1 && n==8 && buf[0]==3);
 assert(au_ipc_commit(&ipc,n)==0);
 assert(au_rf_connected(&rf,mac)==0 && rf.generation==3);
 assert(au_rf_set_max_payload(&rf,2)==0);
 assert(au_rf_daemon_data(&rf,pkt,sizeof pkt)!=0);
 au_ipc_socket_lost(&ipc);
 assert(rf.failed && ipc.failed && !rf.connected);
 puts("AU57X_R24_BT_CONTRACT_OK");return 0;
}
