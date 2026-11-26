/* Host-only R98 loopback fixture. It never calls the QNX native attach branch. */
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
typedef struct {int original,thread,service,owner,transport,profile,attach,withdraw,tick;int got_fd;unsigned calls;int gate;int role;} Fixture;
static int yes_original(void*p){return ((Fixture*)p)->original;}
static int yes_thread(void*p){return ((Fixture*)p)->thread;}
static int yes_service(void*p){return ((Fixture*)p)->service;}
static int yes_owner(void*p){return ((Fixture*)p)->owner;}
static int yes_transport(void*p){return ((Fixture*)p)->transport;}
static int yes_profile(void*p,uint8_t *v){Fixture*f=p;if(!f->profile)return 0;*v=9;return 1;}
static int x3_evidence(void*p,Au57xX3Evidence *e){Fixture*f=p;if(!f->gate)return 0;
 memset(e,0,sizeof *e);e->btstack_sha256="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566";
 e->owner_generation=7;e->original_btstack_process=1;e->mapped_image_hash_verified=1;
 e->owner_thread_verified=1;e->callback_serialization_verified=1;e->original_iap2_service_observed=1;
 e->service_role_known=(uint8_t)f->role;e->rf_arg3_abi_verified=1;e->rf_arg3_value=9;e->sdp_record_verified=1;
 e->sdp_scn_verified=1;e->sdp_scn=9;e->rf_arg3_to_sdp_mapping_verified=1;
 e->callback_bound_to_service=1;e->callback_teardown_verified=1;e->wired_priority_verified=1;
 e->original_mfi_path_preserved=1;return 1;}
static int dummy_read(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;return 1;}
static int dummy_auth(void*p,uint32_t a,const uint8_t b[6]){(void)p;(void)a;(void)b;return 1;}
static int fake_attach(void*p,const MuBtOwner*owner,int fd){Fixture*f=p;struct sockaddr_in sa;socklen_t n=sizeof sa;
 assert(owner->opaque==p);assert(owner->rf_arg3_snapshot_valid==1&&owner->rf_arg3_snapshot==9);
 assert(getpeername(fd,(struct sockaddr*)&sa,&n)==0);assert(sa.sin_addr.s_addr==htonl(INADDR_LOOPBACK));
 assert(fcntl(fd,F_GETFD)&FD_CLOEXEC);f->got_fd=fd;f->calls++;return f->attach;}
static int fake_tick(void*p){Fixture*f=p;f->calls++;return f->tick;}
static int fake_withdraw(void*p,int(*barrier)(void*,const Inbound*),void*v){Fixture*f=p;assert(barrier(v,NULL)==1);f->calls++;return f->withdraw;}
static int barrier(void*p,const Inbound*i){(void)i;return *(int*)p;}
static Au98Hooks hooks(Fixture*f){Au98Hooks h;memset(&h,0,sizeof h);h.opaque=f;
 h.original_image_verified=yes_original;h.on_original_iap2_owner=yes_thread;h.original_iap2_service_verified=yes_service;
 h.attach=fake_attach;h.tick=fake_tick;h.withdraw=fake_withdraw;return h;}
static MuBtOwner owner(Fixture*f){MuBtOwner o;memset(&o,0,sizeof o);o.opaque=f;o.owner_ready=yes_owner;
 o.transport_ready=yes_transport;o.iap2_profile_proven=yes_profile;o.read_live=dummy_read;o.paired_authorized=dummy_auth;
 o.x3_owner_evidence=x3_evidence;return o;}
static int server(unsigned*port){int fd,yes=1;struct sockaddr_in sa;socklen_t n=sizeof sa;
 fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);assert(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)==0);
 memset(&sa,0,sizeof sa);sa.sin_family=AF_INET;sa.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(fd,(struct sockaddr*)&sa,sizeof sa)==0);assert(listen(fd,4)==0);
 assert(getsockname(fd,(struct sockaddr*)&sa,&n)==0);*port=ntohs(sa.sin_port);return fd;}
int main(void){
 Fixture f={1,1,1,1,1,1,0,0,0,-1,0,1,1};Au98Handoff h={0};Au98Hooks k=hooks(&f);MuBtOwner o=owner(&f);
 unsigned port=0;int listen_fd=server(&port),accepted,ok=1;
 assert(au98_publish(NULL,&k,&o,port)<0);
 f.original=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.original=1;
 f.thread=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.thread=1;
 f.service=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.service=1;
 f.profile=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.profile=1;
 f.transport=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.transport=1;
 k.attach=NULL;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);k.attach=fake_attach;
 assert(au98_publish(&h,&k,&o,0)<0&&h.state==AU98_EMPTY);
 f.gate=0;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.gate=1;
 f.role=2;assert(au98_publish(&h,&k,&o,port)<0&&h.state==AU98_EMPTY);f.role=1;
 assert(f.calls==0);
 {unsigned dead_port=0;int dead=server(&dead_port);close(dead);
  assert(au98_publish(&h,&k,&o,dead_port)<0&&h.state==AU98_EMPTY);
  assert(h.native_owner.rf_arg3_snapshot_valid==0);}
 assert(au98_publish(&h,&k,&o,port)==0);accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 assert(h.state==AU98_PUBLISHED&&h.generations==1&&f.calls==1&&f.got_fd==h.fd);
 assert(au98_publish(&h,&k,&o,port)<0);assert(au98_tick(&h)==0&&f.calls==2);
 f.thread=0;assert(au98_tick(&h)<0&&f.calls==2);
 assert(au98_withdraw(&h,barrier,&ok)<0&&h.state==AU98_PUBLISHED);f.thread=1;
 assert(au98_withdraw(&h,barrier,&ok)==0&&h.state==AU98_RETIRED);
 assert(au98_tick(&h)<0&&au98_publish(&h,&k,&o,port)<0);assert(f.calls==3);close(accepted);
 Au98Handoff broken={0};f.attach=-1;assert(au98_publish(&broken,&k,&o,port)==-2);
 accepted=accept(listen_fd,NULL,NULL);assert(accepted>=0);
 assert(broken.state==AU98_POISONED&&au98_publish(&broken,&k,&o,port)<0);
 assert(au98_withdraw(&broken,barrier,&ok)<0);close(accepted);close(listen_fd);
 puts("R98 X3 OWNER GATE + PRE-TCP SNAPSHOT FIXTURE PASS");return 0;
}
