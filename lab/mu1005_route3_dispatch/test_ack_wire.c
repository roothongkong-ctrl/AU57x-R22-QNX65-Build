/* Real local socket + existing sender and receiver; SDK events are fixtures. */
#include "mu1005_socket_pump.h"
#include "cp_step2_ipc_link.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
typedef struct {int fd;size_t sent;} Fixture;
static const uint8_t mac[6]={1,2,3,4,5,6};
static int sdk(void*p,const uint8_t*b,size_t n){
 Fixture*f=p;assert(n&&n<=2&&f->sent+n<=5);
 assert(!memcmp(b,"abcde"+f->sent,n));f->sent+=n;return 0;
}
static int verify(void*p,const uint8_t*m,uint32_t g){(void)p;return g==1&&!memcmp(m,mac,6);}
static int emit(void*p,const uint8_t*b,size_t n){
 Fixture*f=p;return send(f->fd,b,n,0)==(ssize_t)n?0:-1;
}
static void pump(AuIpcStream*s,int fd,struct cp_step2_ipc_link*l,int client){
 unsigned char buf[64];
 for(int i=0;i<16;i++){
  assert(au_socket_step(s,fd,0)>=0);
  ssize_t n=recv(client,buf,sizeof(buf),0);
  if(n>0){assert(cp_step2_ipc_feed(l,buf,(size_t)n)==CP_BT_OK);}
  else {assert(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK));}
 }
}
static void scenario(int ack){
 int fd[2];AuRfIngress rf;AuIpcStream ipc;struct cp_step2_ipc_link l;
 Fixture f;struct cp_step2_ipc_owner o;
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
 for(int i=0;i<2;i++)assert(fcntl(fd[i],F_SETFL,O_NONBLOCK)==0);
 f.fd=fd[1];f.sent=0;
 au_rf_init(&rf,sdk,&f);
 if(ack)assert(au_rf_enable_completion_ack(&rf)==0);
 assert(au_rf_connected(&rf,mac)==0&&au_rf_set_max_payload(&rf,2)==0);
 assert(au_rf_enable_completion_ack(&rf)==-1);
 au_ipc_init(&ipc,&rf);
 o.opaque=&f;o.approve_peer=verify;o.owner_alive=verify;o.emit_frame=emit;
 cp_step2_ipc_init(&l,&o);
 pump(&ipc,fd[0],&l,fd[1]);assert(l.connected);
 assert(cp_step2_ipc_write(&l,(const uint8_t*)"abcde",5)==CP_BT_OK);
 pump(&ipc,fd[0],&l,fd[1]);assert(rf.tx_busy&&f.sent==2&&l.bridge.tx_pending);
 for(int i=0;i<2;i++){
  assert(au_socket_tx_complete(&ipc,1)>=0);
  pump(&ipc,fd[0],&l,fd[1]);assert(l.bridge.tx_pending&&!l.tx_confirmed);
 }
 assert(au_socket_tx_complete(&ipc,1)>=0);
 pump(&ipc,fd[0],&l,fd[1]);
 assert(f.sent==5&&!rf.tx_busy);
 assert(l.tx_confirmed==(unsigned long)ack);
 assert(l.bridge.tx_pending==!ack);
 assert(ipc.rx_frames_committed==0);
 close(fd[0]);close(fd[1]);
}
static void no_ack_on_failure(void){
 AuRfIngress rf;AuRfWire w;Fixture f={-1,0};uint8_t frame[13]={2,0,5,0,1,0,0,0,'a','b','c','d','e'};
 au_rf_init(&rf,sdk,&f);assert(!au_rf_enable_completion_ack(&rf));
 assert(!au_rf_connected(&rf,mac)&&!au_rf_set_max_payload(&rf,2));
 assert(au_rf_take(&rf,&w)==1&&w.bytes[0]==1);
 assert(!au_rf_daemon_data(&rf,frame,sizeof frame));
 assert(!au_rf_send_complete(&rf,1));
 assert(au_rf_take(&rf,&w)==0);
 assert(au_rf_send_complete(&rf,0)==-1);
 assert(au_rf_take(&rf,&w)==0&&!rf.tx_receipt.sequence);
}
int main(void){
 scenario(0);scenario(1);no_ack_on_failure();
 puts("ROUTE3_EXISTING_TYPE4_ACK_REAL_LOCAL_SOCKET_PASS_FIXTURE_ONLY");
 return 0;
}
