/* Host-only real TCP fixture for R98 original-iAP2-owner activation boundary.
 * No original BlueSDK/iPhone/MFi is present in this fixture. */
#define _POSIX_C_SOURCE 200809L
#include "bt_iap2_owner_handoff.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {int original,thread,service,owner,transport,profile,attach,withdraw,tick;int got_fd;unsigned calls;unsigned transport_checks;int revoke_after_check;int revoke_in_tick;} Fixture;
static int yes_original(void*p){return ((Fixture*)p)->original;}
static int yes_thread(void*p){return ((Fixture*)p)->thread;}
static int yes_service(void*p){return ((Fixture*)p)->service;}
static int yes_owner(void*p){return ((Fixture*)p)->owner;}
static int yes_transport(void*p){Fixture*f=p; ++f->transport_checks;return f->transport && !(f->revoke_after_check && f->transport_checks>1);}
/* Fixture mode 1 supplies a fake but valid SCN. 2/3 deliberately return
 * invalid 0/31 despite reporting "proven" and must never open the socket. */
static int yes_profile(void*p,uint8_t *v){
 Fixture*f=p;if(!f->profile)return 0;
 *v=(uint8_t)(f->profile==2?0:f->profile==3?31:5);return 1;
}
static int dummy_read(void*p,uint32_t a,void*b,size_t n){
 Fixture*f=p;uint32_t word=0;
 if(n!=4)return 0;
 switch(a){
 case 0x24dca8u:word=0xe3a02004u;break;
 case 0x24dcacu:word=0xeb00a759u;break;
 case 0x277a18u:word=0xe92d4070u;break;
 case 0x277a6cu:word=0xe5c4608bu;break;
 case 0x277a74u:word=0xe1c468b6u;break;
 default:return 0;
 }
 if(f->profile==4&&a==0x24dca8u)word=0;
 for(unsigned i=0;i<4;i++)((uint8_t*)b)[i]=(uint8_t)(word>>(8*i));
 return 1;
}
static int dummy_auth(void*p,uint32_t a,const uint8_t b[6]){(void)p;(void)a;(void)b;return 1;}
static int fake_attach(void*p,const MuBtOwner*owner,int fd){Fixture*f=p;struct sockaddr_in sa;socklen_t n=sizeof sa;
 assert(owner->opaque==p);
 assert(owner->rf_arg3_snapshot_valid==1u && owner->rf_arg3_snapshot==(owner->iap2_profile_proven?5u:4u));
 assert(getpeername(fd,(struct sockaddr*)&sa,&n)==0);
 assert(sa.sin_addr.s_addr==htonl(INADDR_LOOPBACK));assert(fcntl(fd,F_GETFD)&FD_CLOEXEC);
 f->got_fd=fd;f->calls++;return f->attach;
}
static int fake_tick(void*p){Fixture*f=p;f->calls++;if(f->revoke_in_tick)f->transport=0;return f->tick;}
static int fake_withdraw(void*p,int(*barrier)(void*,const Inbound*),void*v){Fixture*f=p;
 assert(barrier(v,NULL)==1);f->calls++;return f->withdraw;
}
static int barrier(void*p,const Inbound*i){(void)i;return *(int*)p;}
static Au98Hooks hooks(Fixture*f){Au98Hooks h;
 memset(&h,0,sizeof h);h.opaque=f;h.original_image_verified=yes_original;
 h.on_original_iap2_owner=yes_thread;h.original_iap2_service_verified=yes_service;
 h.attach=fake_attach;h.tick=fake_tick;h.withdraw=fake_withdraw;return h;
}
static MuBtOwner owner(Fixture*f){MuBtOwner o;
 memset(&o,0,sizeof o);o.opaque=f;o.owner_ready=yes_owner;
 o.transport_ready=yes_transport;o.iap2_profile_proven=yes_profile;
 o.read_live=dummy_read;o.paired_authorized=dummy_auth;return o;
}
static int server(unsigned*port){int fd,yes=1;struct sockaddr_in sa;socklen_t n=sizeof sa;
 fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);assert(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)==0);
 memset(&sa,0,sizeof sa);sa.sin_family=AF_INET;sa.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(fd,(struct sockaddr*)&sa,sizeof sa)==0);assert(listen(fd,4)==0);
 assert(getsockname(fd,(struct sockaddr*)&sa,&n)==0);*port=ntohs(sa.sin_port);return fd;
}
typedef struct{int fd,allowed;unsigned calls,deny_at;} Witness;
static int witness(void*p,int fd){Witness*w=p;++w->calls;return w->allowed&&fd==w->fd&&w->calls!=w->deny_at;}
static int ack_attach(const MuBtOwner*o,int fd,int(*v)(void*,int),void*ctx){
 Fixture*f=o->opaque;assert(ctx!=o->opaque);assert(v(ctx,fd)==1);
 return fake_attach(f,o,fd);
}
int main(void){
 Fixture f={.original=1,.thread=1,.service=1,.owner=1,.transport=1,.profile=1,.got_fd=-1};
 Au98Hooks k=hooks(&f);MuBtOwner o=owner(&f);Au98Handoff h={0};unsigned port=0;
 int l=server(&port),fd=au98_prepare_daemon_socket(port),ok=1;
 assert(fd>=0);int peer=accept(l,NULL,NULL);assert(peer>=0);
 Witness v={fd,0,0,0};
 assert(au98_adopt_with_ack(&h,&k,&o,fd,port,witness,&v,ack_attach)==-1&&!f.calls);
 v.allowed=1;v.calls=0;v.deny_at=2;
 assert(au98_adopt_with_ack(&h,&k,&o,fd,port,witness,&v,ack_attach)==-1&&!f.calls&&h.state==AU98_EMPTY);
 v.deny_at=0;v.calls=0;
 assert(au98_adopt_with_ack(&h,&k,&o,fd,port,witness,&v,NULL)==-1&&!f.calls);
 assert(au98_adopt_with_ack(&h,&k,&o,fd,port,witness,&v,ack_attach)==0&&f.calls==1&&v.calls==3);
 assert(h.hooks.opaque==&f&&h.native_owner.opaque==&f);
 assert(au98_tick(&h)==0&&au98_withdraw(&h,barrier,&ok)==0);
 close(peer);close(l);
 puts("ROUTE6_8_9_ACK_CONTEXT_HANDOFF_PASS_FIXTURE_ONLY");return 0;
}
