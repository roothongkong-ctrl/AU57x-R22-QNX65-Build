/* Host protocol integration of the two existing transport implementations.
 * No native SDK, iPhone, certificate, or OEM lifecycle is simulated as success. */
#include "tcp_adopt.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <errno.h>
#include "mu1005_socket_pump.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
struct sink { unsigned char data[5035]; size_t used; unsigned calls; };
static int sdk_send_fixture(void *ctx,const uint8_t *p,size_t n){
 struct sink *s=ctx;assert(n<=127 && n<=sizeof s->data-s->used);
 memcpy(s->data+s->used,p,n);s->used+=n;++s->calls;return 0;
}
static void step(AuIpcStream *ipc,struct wcp_oem_graft_io *io,int fd){
 assert(au_socket_step(ipc,fd,1)>=0);assert(wcp_oem_graft_pump(io)==0); (void)poll(NULL,0,1);
}
struct proof_fixture {int fd; unsigned port,peer; int allow;};
static int verify_peer(void *p,int fd,unsigned local,unsigned peer){
 struct proof_fixture *f=p;return f->allow&&fd==f->fd&&local==f->port&&peer==f->peer;
}
int main(void){
 static AuRfIngress rf;static AuIpcStream ipc;static struct wcp_oem_graft_io io;
 struct wcp_graft_lifecycle_event evt;struct sink sink={0};int fd[2];
 unsigned char p[5035],b[5035];const uint8_t mac[]={1,2,3,4,5,6},mac2[]={6,5,4,3,2,1};
 int listener=socket(AF_INET,SOCK_STREAM,0);assert(listener>=0);
 struct sockaddr_in a={0},client={0};socklen_t len=sizeof a;
 a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(listener,(struct sockaddr*)&a,sizeof a)==0&&listen(listener,1)==0);
 assert(getsockname(listener,(struct sockaddr*)&a,&len)==0);
 fd[0]=socket(AF_INET,SOCK_STREAM,0);assert(fd[0]>=0);
 assert(connect(fd[0],(struct sockaddr*)&a,sizeof a)==0);
 len=sizeof client;assert(getsockname(fd[0],(struct sockaddr*)&client,&len)==0);
 fd[1]=accept(listener,NULL,NULL);assert(fd[1]>=0);close(listener);
 struct proof_fixture proof={fd[1],ntohs(a.sin_port),ntohs(client.sin_port),0};
 for(int i=0;i<2;i++){int one=1;assert(setsockopt(fd[i],IPPROTO_TCP,TCP_NODELAY,&one,sizeof one)==0);assert(fcntl(fd[i],F_SETFL,O_NONBLOCK)==0);}
 au_rf_init(&rf,sdk_send_fixture,&sink);au_ipc_init(&ipc,&rf);
 assert(wcp_oem_graft_adopt(&io,fd[1])==-EINVAL);
 assert(wcp_r29_tcp_adopt(&io,fd[1],proof.port,NULL,&proof)==-EINVAL);
 assert(wcp_r29_tcp_adopt(&io,fd[1],proof.port,verify_peer,&proof)==-EACCES);
 proof.allow=1;
 assert(wcp_r29_tcp_adopt(&io,fd[1],proof.port,verify_peer,&proof)==0);
 assert(fcntl(fd[1],F_GETFD)&FD_CLOEXEC);
 puts("PASS real TCP endpoint; unknown peer rejected before transfer");
 assert(au_rf_connected(&rf,mac)==0);
 for(unsigned i=0;i<sizeof p;i++)p[i]=(unsigned char)(i%251);
 assert(au_rf_received(&rf,p,sizeof p)==0);
 for(int i=0;i<12;i++)step(&ipc,&io,fd[0]);
 assert(wcp_oem_graft_event(&io,&evt)==1&&evt.connected&&evt.generation==1&&evt.peer_valid&&!memcmp(evt.peer_address,mac,6));
 assert(wcp_oem_graft_recv(&io,b,sizeof b)==sizeof b&&!memcmp(b,p,sizeof b));
 puts("PASS actual R45 to R29 data, peer, generation");
 /* A full outgoing record reaches R45 before MTU: it must wait. */
 assert(wcp_oem_graft_send(&io,p,sizeof p)==sizeof p);
 for(int i=0;i<12;i++)step(&ipc,&io,fd[0]);
 assert(ipc.pending_count==1&&sink.calls==0);
 assert(au_rf_set_max_payload(&rf,127)==0);
 for(int i=0;i<60;i++){
  step(&ipc,&io,fd[0]);if(rf.tx_busy)assert(au_socket_tx_complete(&ipc,1)>=0);
 }
 assert(!rf.tx_busy&&!ipc.pending_count&&sink.used==sizeof p&&sink.calls==40&&!memcmp(sink.data,p,sizeof p));
 puts("PASS R29 to R45 MTU delay and 40 completion-paced chunks");
 /* Queue old TX while the SDK still owns the first send; disconnect cancels it. */
 memset(&sink,0,sizeof sink);assert(wcp_oem_graft_send(&io,p,127)==127);
 for(int i=0;i<4;i++){step(&ipc,&io,fd[0]);}assert(rf.tx_busy&&sink.calls==1);
 assert(wcp_oem_graft_send(&io,p+127,127)==127);
 for(int i=0;i<4;i++){step(&ipc,&io,fd[0]);}assert(ipc.pending_count==1);
 assert(au_rf_disconnected(&rf)==0);assert(au_socket_tx_complete(&ipc,1)>=0);
 assert(!rf.tx_busy&&!ipc.pending_count&&sink.calls==1);
 for(int i=0;i<4;i++)step(&ipc,&io,fd[0]);
 assert(wcp_oem_graft_event(&io,&evt)==1&&!evt.connected&&evt.generation==1);
 assert(au_rf_connected(&rf,mac2)==0);assert(au_rf_set_max_payload(&rf,127)==0);
 for(int i=0;i<4;i++)step(&ipc,&io,fd[0]);
 assert(wcp_oem_graft_event(&io,&evt)==1&&evt.connected&&evt.generation==3&&!memcmp(evt.peer_address,mac2,6));
 assert(wcp_oem_graft_send(&io,p+254,4)==4);
 for(int i=0;i<4;i++){step(&ipc,&io,fd[0]);if(rf.tx_busy)assert(au_socket_tx_complete(&ipc,1)>=0);}
 assert(sink.calls==2&&sink.used==131&&!memcmp(sink.data+127,p+254,4));
 puts("PASS stale queued data discarded, new peer receives only new bytes");
 /* Old and future wire records have different meanings. */
 {uint8_t stale[]={2,0,1,0,1,0,0,0,0x99};assert(au_ipc_feed(&ipc,stale,sizeof stale)==0&&!ipc.pending_count&&sink.calls==2);
  stale[4]=4;assert(au_ipc_feed(&ipc,stale,sizeof stale)<0&&ipc.failed&&rf.failed&&sink.calls==2);}
 puts("PASS stale frame dropped, future generation rejected");
 wcp_oem_graft_stop(&io);close(fd[0]);return 0;
}
