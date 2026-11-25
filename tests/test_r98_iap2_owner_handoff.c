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

typedef struct {int original,thread,service,owner,transport,profile,attach,withdraw,tick;int got_fd;unsigned calls;unsigned transport_checks;int revoke_after_check;} Fixture;
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
static int dummy_read(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;return 1;}
static int dummy_auth(void*p,uint32_t a,const uint8_t b[6]){(void)p;(void)a;(void)b;return 1;}
static int fake_attach(void*p,const MuBtOwner*owner,int fd){Fixture*f=p;struct sockaddr_in sa;socklen_t n=sizeof sa;
 assert(owner->opaque==p);
 assert(owner->rf_arg3_snapshot_valid==1u && owner->rf_arg3_snapshot==5u);
 assert(getpeername(fd,(struct sockaddr*)&sa,&n)==0);
 assert(sa.sin_addr.s_addr==htonl(INADDR_LOOPBACK));assert(fcntl(fd,F_GETFD)&FD_CLOEXEC);
 f->got_fd=fd;f->calls++;return f->attach;
}
static int fake_tick(void*p){Fixture*f=p;f->calls++;return f->tick;}
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
int main(void){
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
