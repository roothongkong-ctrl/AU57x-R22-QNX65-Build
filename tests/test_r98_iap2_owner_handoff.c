/* Host-only real TCP fixture for R98 original-iAP2-owner activation boundary.
 * No original BlueSDK/iPhone/MFi is present in this fixture. */
#define _POSIX_C_SOURCE 200809L
#include "../project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.h"
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
static void adopt_cases(void){
 Fixture f={.original=1,.thread=1,.service=1,.owner=1,.transport=1,.profile=1,.got_fd=-1};
 Au98Hooks k=hooks(&f);MuBtOwner o=owner(&f);Au98Handoff h={0};unsigned port=0;
 int l=server(&port),fd=socket(AF_INET,SOCK_STREAM,0),ok=1;struct sockaddr_in a={0};
 assert(fd>=0);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons((uint16_t)port);
 assert(!connect(fd,(struct sockaddr*)&a,sizeof a));
 int peer=accept(l,NULL,NULL);assert(peer>=0);
 Witness v={fd,0,0,0};
 assert(au98_adopt(&h,&k,&o,fd,port,witness,&v)==-1&&!v.calls);
 assert(!fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK));
 assert(!fcntl(fd,F_SETFD,FD_CLOEXEC));
 assert(au98_adopt(&h,&k,&o,fd,port,witness,&v)==-1&&!f.calls&&fcntl(fd,F_GETFD)>=0);
 v.allowed=1;v.calls=0;v.deny_at=2;
 assert(au98_adopt(&h,&k,&o,fd,port,witness,&v)==-1&&!f.calls&&h.state==AU98_EMPTY);
 v.deny_at=0;v.calls=0;
 assert(au98_adopt(&h,&k,&o,fd,port==65535?port-1:port+1,witness,&v)==-1&&!v.calls);
 f.thread=0;assert(au98_adopt(&h,&k,&o,fd,port,witness,&v)==-1&&!f.calls);f.thread=1;
 /* Reuse the actual MU1005 credit setting without a separate iAP2
  * profile callback; the fixture reproduces only the stock words. */
 o.iap2_profile_proven=NULL;f.profile=4;
 assert(au98_adopt(&h,&k,&o,fd,port,witness,&v)==-1&&!f.calls);
 f.profile=1;
 assert(au98_adopt(&h,&k,&o,fd,port,witness,&v)==0&&h.fd==fd&&f.calls==1);
 assert(h.native_owner.rf_arg3_snapshot==4u);
 puts("PASS pinned MU1005 stock initial four-credit trial selection");
 assert(au98_withdraw(&h,barrier,&ok)==0&&h.fd==-1);close(peer);close(l);
 puts("PASS prepared daemon fd adoption: no blocking connect and exact attestation");
}
typedef struct {int safe,wired,reclaim;unsigned port;Au98Hooks k;MuBtOwner o;Witness v;} DriveFixture;
static int safe_step(void*p){return ((DriveFixture*)p)->safe;}
static int wired_step(void*p){return ((DriveFixture*)p)->wired;}
static int barrier_step(void*p,const Inbound*i){(void)p;(void)i;return 1;}
static int reclaimed_step(void*p,const Inbound*i){(void)i;return ((DriveFixture*)p)->reclaim;}
static int adopt_step(void*p,Au98Handoff*s,int fd){
 DriveFixture*d=p;return au98_adopt(s,&d->k,&d->o,fd,d->port,witness,&d->v);
}
/* Only a fixture backend; native reclaim is not being simulated as proof. */
static int reclaim_step(Au98Handoff*s,int(*r)(void*,const Inbound*),void*p){
 if(s->state!=AU98_RETIRED||s->fd!=-1||r(p,NULL)!=1)return -1;
 uint32_t gen=s->generations;memset(s,0,sizeof *s);s->generations=gen;s->fd=-1;return 0;
}
static int make_prepared(unsigned port){
 int fd=socket(AF_INET,SOCK_STREAM,0);struct sockaddr_in a={0};
 assert(fd>=0);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons((uint16_t)port);
 assert(!connect(fd,(struct sockaddr*)&a,sizeof a));
 assert(!fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK));assert(!fcntl(fd,F_SETFD,FD_CLOEXEC));return fd;
}
static void drive_cases(void){
 Fixture f={.original=1,.thread=1,.service=1,.owner=1,.transport=1,.profile=1,.got_fd=-1};
 DriveFixture d={.safe=1,.wired=1,.k=hooks(&f),.o=owner(&f)};
 Au98DispatchPolicy p={&d,safe_step,wired_step,barrier_step,reclaimed_step};
 Au98Handoff s={0};int listener=server(&d.port);
 int fd=make_prepared(d.port),peer=accept(listener,NULL,NULL);assert(peer>=0);
 d.v=(Witness){fd,1,0,0};
 #define STEP() au98_drive(&s,&fd,&p,adopt_step,&d,reclaim_step)
 d.safe=0;assert(STEP()==-1&&s.state==AU98_EMPTY&&fd>=0&&!f.calls);
 d.safe=1;d.wired=0;assert(STEP()==0&&fd>=0&&s.state==AU98_EMPTY&&!f.calls);
 d.wired=1;assert(STEP()==0&&fd==-1&&s.state==AU98_PUBLISHED&&s.generations==1);
 assert(STEP()==0);unsigned n=f.calls;
 d.wired=0;f.thread=0;assert(STEP()==-1&&s.state==AU98_PUBLISHED&&f.calls==n);
 f.thread=1;assert(STEP()==0&&s.state==AU98_DRAINING&&f.calls==n);
 f.withdraw=-1;assert(STEP()==-1&&s.fd>=0);
 f.withdraw=0;assert(STEP()==0&&s.state==AU98_RETIRED&&s.fd==-1);
 {unsigned char b;assert(recv(peer,&b,1,0)==0);}close(peer);
 fd=make_prepared(d.port);peer=accept(listener,NULL,NULL);assert(peer>=0);
 d.v=(Witness){fd,1,0,0};d.wired=1;n=f.calls;
 assert(STEP()==-1&&s.state==AU98_RETIRED&&fd>=0&&f.calls==n);
 d.reclaim=1;assert(STEP()==0&&s.state==AU98_EMPTY&&fd>=0&&f.calls==n);
 assert(STEP()==0&&s.state==AU98_PUBLISHED&&fd==-1&&s.generations==2);
 f.transport=0;n=f.calls;assert(STEP()==-1&&s.state==AU98_DRAINING&&f.calls==n);
 assert(STEP()==0&&s.state==AU98_RETIRED&&s.fd==-1);
 {unsigned char b;assert(recv(peer,&b,1,0)==0);}close(peer);
 close(listener);
 #undef STEP
 puts("PASS unified owner lifecycle: wired preemption, verified retirement and second socket generation");
}
int main(void){
 drive_cases();
 adopt_cases();
 Fixture f={.original=1,.thread=1,.service=1,.owner=1,.transport=1,.profile=1,.got_fd=-1};Au98Handoff h={0};
 Au98Hooks k=hooks(&f);MuBtOwner o=owner(&f);
 unsigned port=0;int listen_fd=server(&port),accepted,ok=1;
 assert(au98_publish(NULL,&k,&o,port)<0);
 f.original=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.original=1;
 f.thread=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.thread=1;
 f.service=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.service=1;
 f.profile=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);
 f.profile=2;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);
 f.profile=3;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);
 f.profile=1;
 f.transport=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.transport=1;
 k.attach=NULL;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);k.attach=fake_attach;
 assert(au98_publish(&h,&k,&o,0)<0&&h.state==AU98_EMPTY);
 /* No original SDK call was used before every independent authority existed. */
 assert(f.calls==0);
 /* Revoke after the first check; actual TCP connection must be closed
  * without attaching, and a fresh verified owner can retry. */
 f.transport_checks=0;f.revoke_after_check=1;
 assert(au98_publish(&h,&k,&o,port)<0 && h.state==AU98_EMPTY && f.calls==0);
 accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 {unsigned char byte;assert(recv(accepted,&byte,1,0)==0);}
 close(accepted);f.revoke_after_check=0;f.transport_checks=0;
 puts("PASS owner revoked during TCP connection: no native attach");
 assert(au98_publish(&h,&k,&o,port)==0);
 accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 assert(h.state==AU98_PUBLISHED&&h.generations==1&&f.calls==1&&f.got_fd==h.fd);
 /* A proof provider changing its reply after setup cannot change the
  * session-bound value that native_binding uses. */
 f.profile=3;
 assert(h.native_owner.rf_arg3_snapshot_valid==1u && h.native_owner.rf_arg3_snapshot==5u);
 assert(au98_publish(&h,&k,&o,port)<0);
 assert(au98_tick(&h)==0&&f.calls==2);
 f.profile=1;
 f.thread=0;assert(au98_tick(&h)<0&&f.calls==2);
 assert(au98_withdraw(&h,barrier,&ok)<0&&h.state==AU98_PUBLISHED);
 f.thread=1;
 assert(au98_withdraw(&h,barrier,&ok)==0&&h.state==AU98_RETIRED);
 assert(au98_tick(&h)<0&&au98_publish(&h,&k,&o,port)<0);
 assert(f.calls==3);close(accepted);
 /* A clean transport failure after successful attach must still permit
  * owner-certified retirement; no retry of native registration. */
 Au98Handoff drained={0};f.tick=-1;
 assert(au98_publish(&drained,&k,&o,port)==0);
 accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 assert(au98_tick(&drained)==-2&&drained.state==AU98_DRAINING);
 assert(au98_tick(&drained)<0&&au98_publish(&drained,&k,&o,port)<0);
 f.thread=0;assert(au98_withdraw(&drained,barrier,&ok)<0&&drained.state==AU98_DRAINING);
 f.thread=1;f.withdraw=-1;
 assert(au98_withdraw(&drained,barrier,&ok)==-1&&drained.state==AU98_DRAINING);
 assert(fcntl(drained.fd,F_GETFD)>=0);
 f.withdraw=0;assert(au98_withdraw(&drained,barrier,&ok)==0&&drained.state==AU98_RETIRED&&drained.fd==-1);
 {unsigned char byte;assert(recv(accepted,&byte,1,0)==0);}
 close(accepted);f.tick=0;
 puts("PASS successful attach -> transport failure -> certified withdrawal");
 /* Real loopback remains open until verified withdrawal; a revoked lease
  * must never restart forwarding merely because the proof recovers. */
 for(int which=0;which<4;which++){
  Au98Handoff lost={0};unsigned n;
  assert(au98_publish(&lost,&k,&o,port)==0);
  accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
  n=f.calls;
  if(which==0)f.transport=0;
  if(which==1)f.service=0;
  if(which==2)f.owner=0;
  if(which==3)f.revoke_in_tick=1;
  assert(au98_tick(&lost)==-2&&lost.state==AU98_DRAINING);
  assert(f.calls==n+(which==3?1u:0u));
  f.transport=f.service=f.owner=1;f.revoke_in_tick=0;
  n=f.calls;
  assert(au98_tick(&lost)<0&&f.calls==n);
  assert(au98_publish(&lost,&k,&o,port)<0);
  assert(au98_withdraw(&lost,barrier,&ok)==0&&lost.fd==-1);
  {unsigned char byte;assert(recv(accepted,&byte,1,0)==0);}
  close(accepted);
 }
 puts("PASS owner/transport lease revoked before or during tick; no stale resume");
 Au98Handoff uncertain={0};f.withdraw=-2;
 assert(au98_publish(&uncertain,&k,&o,port)==0);
 accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 assert(au98_withdraw(&uncertain,barrier,&ok)==-2&&uncertain.state==AU98_POISONED);
 assert(au98_withdraw(&uncertain,barrier,&ok)<0&&fcntl(uncertain.fd,F_GETFD)>=0);
 close(accepted);f.withdraw=0;
 puts("PASS ambiguous withdrawal retains descriptor and cannot retry");
 /* An ambiguous native registration failure is POISONED, never retried.
  * The service may have captured native pointers before returning error. */
 Au98Handoff broken={0};f.attach=-1;
 assert(au98_publish(&broken,&k,&o,port)==-2);
 accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 assert(broken.state==AU98_POISONED&&au98_publish(&broken,&k,&o,port)<0);
 assert(au98_withdraw(&broken,barrier,&ok)<0);
 close(accepted);close(listen_fd);
 puts("R98 REAL LOOPBACK OWNER GATE PASS: authority, live fd, rollback, poison");
 return 0;
}
