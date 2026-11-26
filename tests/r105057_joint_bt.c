/* R105.057: join the existing R98 worker/owner lifecycle to the actual
 * R45 ARM-record decoder and bidirectional RFCOMM/TCP stream in one run.
 * SDK operations, peer authorization and callback records are fixtures.
 * No production OEM startup, real Bluetooth or MFi proof. NO INSTALL. */
#define main r105_reference_native_event_main
#include "r14_test_native_event.c"
#undef main
#include "bt_socket_worker.h"
#include "bt_notify_bridge.h"
#include "bt_queue_pulse.h"
#include <time.h>
typedef struct {
 MuBtSocketWorker worker; MuNotifyGate gate; MuBtPulse pulse;
 Au98Handoff h; Au98Hooks hooks; MuBtOwner owner; Au98DispatchPolicy policy;
 pthread_t owner_tid; int fd,wired,released,rc,originals,dispatches;
} Joint;
static int j_owner(void*p){return pthread_equal(((Joint*)p)->owner_tid,pthread_self())!=0;}
static int j_wired(void*p){return ((Joint*)p)->wired;}
static int j_read(void*p,uint32_t a,void*out,size_t n){(void)p;(void)a;(void)out;(void)n;return 0;}
static int j_profile(void*p,uint8_t*v){if(!j_owner(p))return 0;*v=4;return 1;}
static int j_register(void*p,uint8_t*s,uint16_t v){assert(v==4);return reg(p,s,0);}
static int j_verified(void*p,int fd){return j_owner(p)&&fd==((Joint*)p)->fd;}
static int j_attach(void*p,const MuBtOwner*o,int fd){
 Joint*j=p;assert(j_owner(p)&&j->originals>0&&o->rf_arg3_snapshot_valid==1&&o->rf_arg3_snapshot==4);
 InboundOps ops={0};
 ops.reg=j_register;ops.unreg=unreg;ops.add=rec;ops.remove=rec;ops.accept=accept_rf;
 ops.limit=limit;ops.send=send_rf;ops.disconnect=disconnect_rf;ops.credit=credit;
 ops.addr=addr;ops.certify=yes;ops.ready=yes;ops.opaque=&peerops;ops.initial_rx_credits=4;
 if(ir5_init(&b,&ops,mu38_resolve_peer,fd,0x1234))return -1;
 return ir5_publish(&b);
}
static int j_tick(void*p){assert(j_owner(p));return ir5_tick(&b);}
static int j_barrier(void*p,const Inbound*i){return j_owner(p)&&i==&b.service&&!b.native_link&&!b.service.busy;}
static int j_withdraw(void*p,int(*barrier)(void*,const Inbound*),void*ctx){
 assert(j_owner(p));int rc=ir5_drain(&b);return rc?rc:ir5_withdraw(&b,barrier,ctx);
}
static int j_released(void*p,const Inbound*i){return j_owner(p)&&((Joint*)p)->released&&i==&b.service&&b.service.state==IN_RETIRED;}
static int j_reclaim(Au98Handoff*h,int(*reclaimed)(void*,const Inbound*),void*p){
 if(h->state!=AU98_RETIRED||h->fd!=-1||!reclaimed(p,&b.service))return -1;
 uint32_t g=h->generations;memset(h,0,sizeof *h);h->fd=-1;h->generations=g;
 memset(&b,0,sizeof b);sent=used=credits=decision=disconnect_count=0;
 return 0;
}
static int j_adopt(void*p,Au98Handoff*h,int fd){Joint*j=p;return au98_adopt(h,&j->hooks,&j->owner,fd,57342,j_verified,j);}
static void j_original(void*p,unsigned char value){Joint*j=p;assert(value==1);++j->originals;}
static void j_after(void*p){
 Joint*j=p;assert(j_owner(p)&&j->originals>j->dispatches);++j->dispatches;
 if(j->h.state==AU98_EMPTY&&j->fd<0&&j->wired)(void)mu_bt_worker_poll(&j->worker,&j->fd);
 j->rc=au98_drive(&j->h,&j->fd,&j->policy,j_adopt,j,j_reclaim);
}
static int j_execute(void*p){Joint*j=p;mu_notify_call(&j->gate,j_original,j,1);return 1;}
static void j_pulse(Joint*j){
 assert(mu_bt_pulse_request(&j->pulse)==1);assert(!mu_bt_pulse_request(&j->pulse));
 assert(mu_bt_pulse_execute(&j->pulse,j_execute,j)==1);mu_bt_pulse_release(&j->pulse);
}
static void j_pause(void){struct timespec t={0,1000000L};(void)nanosleep(&t,NULL);}
static void j_published(Joint*j){
 for(int i=0;i<3000&&j->h.state!=AU98_PUBLISHED;i++){j_pulse(j);j_pause();}
 assert(j->h.state==AU98_PUBLISHED&&j->fd==-1&&ir5_discovery_ready(&b)==1);
}
static int j_listener(void){
 int l=socket(AF_INET,SOCK_STREAM,0),one=1;assert(l>=0);
 struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons(57342);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(!setsockopt(l,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one));
 assert(!bind(l,(struct sockaddr*)&a,sizeof a)&&!listen(l,4));return l;
}
static void j_session(Joint*j,int l,unsigned round){
 uint8_t mac[6]={1,2,3,4,5,6},r[14];
 j_published(j);assert(j->h.generations==round);
 struct pollfd x={l,POLLIN,0};assert(poll(&x,1,1000)==1);
 int remote=accept(l,NULL,NULL);assert(remote>=0);
 peer_allowed=0;assert(event(mac,1,0,NULL,0,0)==0&&decision==0);peer_allowed=1;
 assert(event(mac,1,0,NULL,0,0)==1&&decision==1);
 assert(!event(mac,2,0,NULL,0,0));j_pulse(j);read_full(remote,r,14);
 assert(r[0]==1&&r[2]==6&&!memcmp(r+8,mac,6));
 assert(!event(mac,5,0,(const uint8_t*)"hello",5,0));j_pulse(j);
 read_full(remote,r,13);assert(r[0]==2&&r[2]==5&&!memcmp(r+8,"hello",5));
 uint8_t reply[]={2,0,5,0,1,0,0,0,'a','b','c','d','e'};
 assert(send(remote,reply,3,0)==3);j_pulse(j);assert(!sent);
 assert(send(remote,reply+3,sizeof reply-3,0)==(ssize_t)(sizeof reply-3));
 for(int i=0;i<1000&&!sent;i++){j_pulse(j);j_pause();}assert(sent==1);
 for(int i=0;i<3;i++){assert(!event(mac,6,0,NULL,0,addr(&b.service.packet)));j_pulse(j);}
 assert(sent==3&&lengths[0]==2&&lengths[1]==2&&lengths[2]==1&&!memcmp(bytes,"abcde",5)&&!b.rf.tx_busy);
 /* USB preempts active wireless; no teardown before simulated OEM close. */
 j->wired=0;j_pulse(j);assert(j->h.state==AU98_DRAINING);
 j_pulse(j);assert(j->h.state==AU98_DRAINING&&j->h.fd>=0&&disconnect_count==1);
 assert(!event(mac,4,0,NULL,0,0));j_pulse(j);
 assert(j->h.state==AU98_RETIRED&&j->h.fd==-1);
 x.fd=remote;x.events=POLLIN;x.revents=0;assert(poll(&x,1,1000)==1);
 char c;assert(recv(remote,&c,1,0)==0);close(remote);
 j->released=0;j_pulse(j);assert(j->h.state==AU98_RETIRED);
 j->released=1;j_pulse(j);assert(j->h.state==AU98_EMPTY);
 j->wired=1;
}
int main(void){
 Joint j={0};j.owner_tid=pthread_self();j.fd=j.h.fd=-1;j.wired=1;j.pulse.active=1;
 j.hooks=(Au98Hooks){&j,j_owner,j_owner,j_owner,j_attach,j_tick,j_withdraw};
 j.owner.opaque=&j;j.owner.owner_ready=j_owner;j.owner.transport_ready=j_owner;
 j.owner.read_live=j_read;j.owner.iap2_profile_proven=j_profile;
 j.policy=(Au98DispatchPolicy){&j,j_owner,j_wired,j_barrier,j_released};
 j.gate=(MuNotifyGate){&j,j_owner,j_owner,j_after,0};
 int l=j_listener();assert(!mu_bt_worker_start(&j.worker,57342));
 j_session(&j,l,1);j_session(&j,l,2);
 mu_bt_pulse_disable(&j.pulse);assert(mu_bt_pulse_idle(&j.pulse));
 assert(!mu_bt_worker_stop(&j.worker));close(l);
 assert(j.originals==j.dispatches&&!j.gate.inside);
 puts("R105057_JOINT_OWNER_RFCOMM_TWO_SESSIONS_PASS_FIXTURE_ONLY_NO_INSTALL");return 0;
}
