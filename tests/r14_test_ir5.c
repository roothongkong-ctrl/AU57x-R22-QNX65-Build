#include "inbound_r5.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
static InboundR5 b;
static int sent,credits,decision,peer_allowed=1;
static unsigned lengths[8];static uint8_t bytes[32];static unsigned used;
static int yes(void*p){(void)p;return 1;}
static uint32_t addr(const void*p){return (uint32_t)(0x10000+(const uint8_t*)p-(const uint8_t*)&b);}
static int reg(void*p,uint8_t*s,uint16_t v){assert(p==&b.service.rf&&v==0);*s=3;return 0;}
static int unreg(void*p,uint8_t*s){(void)p;(void)s;return 0;}
static int rec(void*p){assert(p==&b.service.sdp||p==&b.service.carplay);return 0;}
static int accept_rf(void*p,int v){assert(p==&b.service.rf);decision=v;return 2;}
static int limit(void*p){assert(p==&b.service.rf);return 2;}
static int send_rf(void*p,void*q){assert(p==&b.service.rf&&q==&b.service.packet);unsigned n=b.service.packet.words[3]&65535;assert(n&&n<=2);lengths[sent++]=n;memcpy(bytes+used,b.service.tx,n);used+=n;return 2;}
static int disconnect_rf(void*p){(void)p;return 2;}
static int credit(void*p,int n){assert(p==&b.service.rf);credits+=n;return 0;}
static int resolve(void*p,const void*ch,const void*ev,uint8_t out[6]){(void)p;assert(ch==&b.service.rf);if(!peer_allowed)return 0;memcpy(out,ev,6);return 1;}
static void tick(void){assert(ir5_tick(&b)==0);}
static void read_full(int fd,uint8_t *p,size_t n){while(n){struct pollfd x={fd,POLLIN,0};assert(poll(&x,1,500)>0);ssize_t k=recv(fd,p,n,0);assert(k>0);p+=k;n-=(size_t)k;}}
static int event(const uint8_t*peer,unsigned code,unsigned status,const uint8_t*p,unsigned n,uint32_t pkt){return ir5_event(&b,&b.service.rf,peer,code,status,p,n,pkt);}
int main(void){
 int server=socket(AF_INET,SOCK_STREAM,0),fd=socket(AF_INET,SOCK_STREAM,0);assert(server>=0&&fd>=0);int reuse=1;assert(!setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse));struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(57342);assert(!bind(server,(struct sockaddr*)&a,sizeof a)&&!listen(server,1));assert(!connect(fd,(struct sockaddr*)&a,sizeof a));int remote=accept(server,NULL,NULL);assert(remote>=0);
 InboundOps ops={0};ops.reg=reg;ops.unreg=unreg;ops.add=rec;ops.remove=rec;ops.accept=accept_rf;ops.limit=limit;ops.send=send_rf;ops.disconnect=disconnect_rf;ops.credit=credit;ops.addr=addr;ops.certify=yes;ops.ready=yes;
 assert(!ir5_init(&b,&ops,resolve,fd,0x1234));assert(!ir5_publish(&b));uint8_t peer[6]={1,2,3,4,5,6},unknown[6]={0};
 assert(event(unknown,1,0,NULL,0,0)==0&&decision==0);peer_allowed=0;assert(event(peer,1,0,NULL,0,0)==0&&decision==0);peer_allowed=1;
 assert(event(peer,1,0,NULL,0,0)==1&&decision==1);assert(!event(peer,2,0,NULL,0,0)&&credits==4);tick();uint8_t r[14];read_full(remote,r,14);assert(r[0]==1&&r[2]==6&&r[4]==1&&!memcmp(r+8,peer,6));
 assert(!event(peer,5,0,(const uint8_t*)"hello",5,0)&&credits==4);tick();assert(credits==5);read_full(remote,r,13);assert(r[0]==2&&r[2]==5&&!memcmp(r+8,"hello",5));
 /* Burst of four outstanding frames: no credit grant before TCP commit. */
 for(int i=0;i<4;i++)assert(!event(peer,5,0,(const uint8_t*)"x",1,0));
 assert(credits==5);
 for(int i=0;i<4;i++){tick();read_full(remote,r,9);assert(r[0]==2&&r[2]==1&&r[8]=='x');assert(credits==6+i);}
 uint8_t wire[]={2,0,5,0,1,0,0,0,'a','b','c','d','e'};assert(send(remote,wire,3,0)==3);tick();assert(!sent);assert(send(remote,wire+3,sizeof wire-3,0)==(ssize_t)(sizeof wire-3));for(int i=0;i<500&&!sent;i++){tick();struct pollfd x={fd,POLLIN,0};(void)poll(&x,1,1);}assert(sent==1&&lengths[0]==2&&b.service.busy);
 assert(event(peer,6,0,NULL,0,addr(&b.service.packet)+4)<0);assert(sent==1);assert(!event(peer,6,0,NULL,0,addr(&b.service.packet)));assert(sent==1);tick();assert(sent==2);assert(!event(peer,6,0,NULL,0,addr(&b.service.packet)));tick();assert(sent==3&&lengths[2]==1);assert(!event(peer,6,0,NULL,0,addr(&b.service.packet)));tick();assert(!b.rf.tx_busy&&!memcmp(bytes,"abcde",5));
 assert(!event(peer,4,0,NULL,0,0));assert(ir5_tick(&b)<0);read_full(remote,r,8);assert(r[0]==3&&r[2]==0&&r[4]==1);assert(b.dead);close(remote);close(fd);close(server);
 puts("PASS: TCP 57342, verified-peer fixture, CONNECTED MAC, RX payload, fragmented DATA, MTU 2/2/1, deferred event6, DISCONNECTED; OEM substitutes, no hardware.");return 0;
}
