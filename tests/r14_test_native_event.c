#include "native_event.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
static InboundR5 b;
static int trace_mode;
static int sent,credits,decision,peer_allowed=1,disconnect_count;
static unsigned lengths[8];static uint8_t bytes[32];static unsigned used;
static int yes(void*p){(void)p;return 1;}
static uint32_t addr(const void*p){if(trace_mode&&p==&b.service.packet)return 0x801200;return (uint32_t)(0x10000+(const uint8_t*)p-(const uint8_t*)&b);}
static int reg(void*p,uint8_t*s,uint16_t v){assert(p==&b.service.rf&&v==0);*s=3;return 0;}
static int unreg(void*p,uint8_t*s){(void)p;(void)s;return 0;}
static int rec(void*p){assert(p==&b.service.sdp||p==&b.service.carplay);return 0;}
static int accept_rf(void*p,int v){assert(p==&b.service.rf);decision=v;return 2;}
static int limit(void*p){assert(p==&b.service.rf);return 2;}
static int send_rf(void*p,void*q){assert(p==&b.service.rf&&q==&b.service.packet);unsigned n=b.service.packet.words[3]&65535;assert(n&&n<=2);lengths[sent++]=n;memcpy(bytes+used,b.service.tx,n);used+=n;return 2;}
static int disconnect_rf(void*p){assert(p==&b.service.rf);disconnect_count++;return 2;}
static int credit(void*p,int n){assert(p==&b.service.rf);credits+=n;return 0;}
static uint8_t live_mac[6]={1,2,3,4,5,6};static int link_match=1,read_ok=1;
static const uint8_t *live_data;static unsigned live_len;
static int assoc(void*p,const void*ch,uint32_t token,unsigned code){(void)p;assert(ch==&b.service.rf);return link_match&&token==(trace_mode?0x801100:0x20000)&&(code==1||code==2);}
static int rd(void*p,uint32_t token,void*out,size_t n){(void)p;if(!read_ok)return 0;if(token==(trace_mode?0x80110d:0x2000d)&&n==6){memcpy(out,live_mac,6);return 1;}if(token==(trace_mode?0x801403:0x30000)&&n==live_len&&live_data){memcpy(out,live_data,n);return 1;}return 0;}
static int auth(void*p,uint32_t token,const uint8_t mac[6]){(void)p;return peer_allowed&&token==(trace_mode?0x801100:0x20000)&&!memcmp(mac,live_mac,6);}
static Mu38PeerOps peerops={NULL,assoc,rd,auth};
static void tick(void){assert(ir5_tick(&b)==0);}
static void read_full(int fd,uint8_t *p,size_t n){while(n){struct pollfd x={fd,POLLIN,0};assert(poll(&x,1,500)>0);ssize_t k=recv(fd,p,n,0);assert(k>0);p+=k;n-=(size_t)k;}}
static int event(const uint8_t*peer,unsigned code,unsigned status,const uint8_t*p,unsigned n,uint32_t pkt){
 uint8_t e[16]={0};uint32_t token= code==6?pkt:code==5?0x30000:0x20000;
 if(code==1||code==2)memcpy(live_mac,peer,6);
 live_data=p;live_len=n;e[0]=code;e[2]=status;e[3]=status>>8;e[8]=n;e[9]=n>>8;
 for(unsigned i=0;i<4;i++)e[12+i]=(uint8_t)(token>>(8*i));
 int r=mu38_dispatch(&b,&peerops,&b.service.rf,e);live_data=NULL;live_len=0;return r;
}
/* Exact target registry offsets, fixture memory only. */
typedef struct { uint32_t channel,association; unsigned reads; int bad_got,bad_member,bad_flags,bad_index,change; } RegFixture;
static int reg_read(void *p,uint32_t a,void *out,size_t n){
 RegFixture *f=p;
 const uint32_t base=0x2d58d4u,link=base+2u*0xaf8u,row=link+6u*16u;
 uint32_t v;
 if(a==f->channel+0x5du&&n==2){uint8_t ix[2]={2,(uint8_t)(f->bad_index?17:5)};memcpy(out,ix,2);return 1;}
 if(a==row+0x1bc5c8u&&n==1){*(uint8_t*)out=f->bad_flags?0:1;return 1;}
 if(n!=4)return 0;
 if(a==0x2c1964u)v=f->bad_got?base+4:base;
 else if(a==row+0x1bc5ccu)v=f->bad_member?f->channel+4:f->channel;
 else if(a==link+0x1bc6ecu){v=f->association;if(f->change&&f->reads++)v+=4;}
 else return 0;
 for(unsigned i=0;i<4;i++)((uint8_t*)out)[i]=(uint8_t)(v>>(8*i));
 return 1;
}
static void registry_test(void){
 RegFixture f={0x700000u,0x710000u,0,0,0,0,0,0};
 assert(mu38_registry_association(f.channel,f.association,reg_read,&f)==1);
 assert(mu38_registry_association(f.channel,f.association+4,reg_read,&f)==0);
 f.bad_got=1;assert(!mu38_registry_association(f.channel,f.association,reg_read,&f));f.bad_got=0;
 f.bad_member=1;assert(!mu38_registry_association(f.channel,f.association,reg_read,&f));f.bad_member=0;
 f.bad_flags=1;assert(!mu38_registry_association(f.channel,f.association,reg_read,&f));f.bad_flags=0;
 f.bad_index=1;assert(!mu38_registry_association(f.channel,f.association,reg_read,&f));f.bad_index=0;
 f.change=1;assert(!mu38_registry_association(f.channel,f.association,reg_read,&f));
 puts("PASS MU1005 native channel registry membership and association without SDK re-lock (fixture)");
}
int main(int argc,char **argv){
 registry_test();
 trace_mode=argc==3&&!strcmp(argv[1],"native-trace");
 int server=socket(AF_INET,SOCK_STREAM,0),fd=socket(AF_INET,SOCK_STREAM,0);assert(server>=0&&fd>=0);int reuse=1;assert(!setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse));struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(57342);assert(!bind(server,(struct sockaddr*)&a,sizeof a)&&!listen(server,1));assert(!connect(fd,(struct sockaddr*)&a,sizeof a));int remote=accept(server,NULL,NULL);assert(remote>=0);
 InboundOps ops={0};ops.reg=reg;ops.unreg=unreg;ops.add=rec;ops.remove=rec;ops.accept=accept_rf;ops.limit=limit;ops.send=send_rf;ops.disconnect=disconnect_rf;ops.credit=credit;ops.addr=addr;ops.certify=yes;ops.ready=yes;ops.opaque=&peerops;
 assert(!ir5_init(&b,&ops,mu38_resolve_peer,fd,0x1234));assert(!ir5_publish(&b));uint8_t peer[6]={1,2,3,4,5,6},unknown[6]={0};
 if(trace_mode){
  uint8_t records[5][16],r[14];FILE*f=fopen(argv[2],"rb");assert(f&&fread(records,1,sizeof records,f)==sizeof records&&fgetc(f)==EOF);fclose(f);
  assert(records[0][0]==1&&records[1][0]==2&&records[2][0]==5&&records[3][0]==6&&records[4][0]==4);
  assert(mu38_dispatch(&b,&peerops,&b.service.rf,records[0])==1&&decision==1);
  assert(!mu38_dispatch(&b,&peerops,&b.service.rf,records[1])&&credits==4);tick();read_full(remote,r,14);assert(r[0]==1&&r[2]==6&&!memcmp(r+8,peer,6));
  live_data=(const uint8_t*)"hello";live_len=5;assert(!mu38_dispatch(&b,&peerops,&b.service.rf,records[2]));live_data=NULL;live_len=0;
  tick();read_full(remote,r,13);assert(r[0]==2&&r[2]==5&&!memcmp(r+8,"hello",5)&&credits==5);
  uint8_t wire[]={2,0,5,0,1,0,0,0,'a','b','c','d','e'};assert(send(remote,wire,sizeof wire,0)==sizeof wire);
  for(int i=0;i<500&&!sent;i++){tick();struct pollfd x={fd,POLLIN,0};(void)poll(&x,1,1);}assert(sent==1);
  for(int i=0;i<3;i++){assert(!mu38_dispatch(&b,&peerops,&b.service.rf,records[3]));tick();}
  assert(sent==3&&lengths[0]==2&&lengths[1]==2&&lengths[2]==1&&!memcmp(bytes,"abcde",5));
  assert(!mu38_dispatch(&b,&peerops,&b.service.rf,records[4]));assert(ir5_tick(&b)<0);read_full(remote,r,8);assert(r[0]==3);close(remote);close(fd);close(server);
  puts("PASS: exact 16-byte ARM interpreter callback snapshots -> C bridge -> TCP; simulated device memory, auth and TX completion scheduling; not QNX.");return 0;
 }
 assert(event(unknown,1,0,NULL,0,0)==0&&decision==0);peer_allowed=0;assert(event(peer,1,0,NULL,0,0)==0&&decision==0);peer_allowed=1;link_match=0;assert(event(peer,1,0,NULL,0,0)==0&&decision==0);link_match=1;read_ok=0;assert(event(peer,1,0,NULL,0,0)==0&&decision==0);read_ok=1;
 assert(event(peer,1,0,NULL,0,0)==1&&decision==1);
 if(argc>1){
  /* Different identity at event2: shut bridge and queue OEM disconnect on owner tick only. */
  peer_allowed=0;assert(event(peer,2,0,NULL,0,0)<0);assert(b.dead&&b.native_link&&!disconnect_count);
  assert(ir5_tick(&b)<0&&disconnect_count==1&&b.disconnect_rc==2);
  assert(ir5_tick(&b)<0&&disconnect_count==1);
  assert(event(peer,4,0,NULL,0,0)<0&&!b.native_link);
  assert(ir5_tick(&b)<0&&disconnect_count==1);
  close(remote);close(fd);close(server);puts("PASS: identity rejection after native open, owner-only disconnect exactly once; no storage reuse");return 0;
 }
 assert(!event(peer,2,0,NULL,0,0)&&credits==4);tick();uint8_t r[14];read_full(remote,r,14);assert(r[0]==1&&r[2]==6&&r[4]==1&&!memcmp(r+8,peer,6));
 assert(!event(peer,5,0,(const uint8_t*)"hello",5,0)&&credits==4);tick();assert(credits==5);read_full(remote,r,13);assert(r[0]==2&&r[2]==5&&!memcmp(r+8,"hello",5));
 /* Burst of four outstanding frames: no credit grant before TCP commit. */
 for(int i=0;i<4;i++)assert(!event(peer,5,0,(const uint8_t*)"x",1,0));
 assert(credits==5);
 for(int i=0;i<4;i++){tick();read_full(remote,r,9);assert(r[0]==2&&r[2]==1&&r[8]=='x');assert(credits==6+i);}
 uint8_t wire[]={2,0,5,0,1,0,0,0,'a','b','c','d','e'};assert(send(remote,wire,3,0)==3);tick();assert(!sent);assert(send(remote,wire+3,sizeof wire-3,0)==(ssize_t)(sizeof wire-3));for(int i=0;i<500&&!sent;i++){tick();struct pollfd x={fd,POLLIN,0};(void)poll(&x,1,1);}assert(sent==1&&lengths[0]==2&&b.service.busy);
 assert(event(peer,6,0,NULL,0,addr(&b.service.packet)+4)<0);assert(sent==1);assert(!event(peer,6,0,NULL,0,addr(&b.service.packet)));assert(sent==1);tick();assert(sent==2);assert(!event(peer,6,0,NULL,0,addr(&b.service.packet)));tick();assert(sent==3&&lengths[2]==1);assert(!event(peer,6,0,NULL,0,addr(&b.service.packet)));tick();assert(!b.rf.tx_busy&&!memcmp(bytes,"abcde",5));
 assert(!event(peer,4,0,NULL,0,0));assert(ir5_tick(&b)<0);read_full(remote,r,8);assert(r[0]==3&&r[2]==0&&r[4]==1);assert(b.dead&&!disconnect_count);close(remote);close(fd);close(server);
 puts("PASS: TCP 57342, raw ARM32 callback decoder, association/authorization/read checks, simulated live peer, CONNECTED MAC, RX payload, fragmented DATA, MTU 2/2/1, deferred event6, DISCONNECTED; OEM substitutes, no hardware.");return 0;
}
