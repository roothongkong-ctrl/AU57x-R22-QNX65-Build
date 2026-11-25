/* SPDX-License-Identifier: GPL-3.0-only
 * One real QNX TCP/pthread session through worker, original-first callback
 * sequencing and Au98 state machine; OEM calls deliberately substituted.
 * QNX native fixed-address binding is NOT invoked. */
#define _POSIX_C_SOURCE 200809L
#include "bt_notify_bridge.h"
#include "bt_socket_worker.h"
#include <assert.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef struct {
 MuBtSocketWorker worker;Au98Handoff s;Au98Hooks hooks;MuBtOwner owner;
 Au98DispatchPolicy policy;MuNotifyGate gate;
 pthread_t tid;int prepared,listener,peer,wired,release,rc;
 unsigned originals,dispatches,attach,ticks,withdraws,reclaims;uint32_t gen;
} Fixture;
static int on_owner(void*p){return pthread_equal(((Fixture*)p)->tid,pthread_self())!=0;}
static int yes(void*p){return on_owner(p);}
static int wired(void*p){return ((Fixture*)p)->wired;}
static int profile(void*p,uint8_t*v){if(!on_owner(p))return 0;*v=4;return 1;}
static int rd(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;return 0;}
static int auth(void*p,uint32_t a,const uint8_t m[6]){(void)p;(void)a;(void)m;return 0;}
static int verified(void*p,int fd){
 Fixture*f=p;struct sockaddr_in a;socklen_t n=sizeof a;
 return on_owner(p)&&fd==f->prepared&&!getpeername(fd,(struct sockaddr*)&a,&n)&&a.sin_port==htons(57342);
}
static int attach(void*p,const MuBtOwner*o,int fd){
 Fixture*f=p;assert(on_owner(p)&&o->rf_arg3_snapshot==4&&o->rf_arg3_snapshot_valid==1);
 assert(fd==f->prepared&&f->s.fd==fd&&f->originals>0);
 ++f->attach;return 0;
}
static int tick(void*p){
 Fixture*f=p;assert(on_owner(p)&&f->s.state==AU98_PUBLISHED);++f->ticks;return 0;
}
static int barrier(void*p,const Inbound*i){(void)i;return on_owner(p);}
static int released(void*p,const Inbound*i){(void)i;return on_owner(p)&&((Fixture*)p)->release;}
static int withdraw(void*p,int(*b)(void*,const Inbound*),void*c){
 Fixture*f=p;assert(on_owner(p)&&b(c,NULL)==1);++f->withdraws;return 0;
}
static int adopt(void*p,Au98Handoff*s,int fd){
 Fixture*f=p;return au98_adopt(s,&f->hooks,&f->owner,fd,57342,verified,f);
}
static int reclaim(Au98Handoff*s,int(*r)(void*,const Inbound*),void*p){
 Fixture*f=p;assert(on_owner(p)&&s->fd==-1&&s->state==AU98_RETIRED);
 if(r(p,NULL)!=1)return -1;
 uint32_t gen=s->generations;memset(s,0,sizeof *s);s->fd=-1;s->generations=gen;++f->reclaims;return 0;
}
static void original(void*p,unsigned char value){Fixture*f=p;assert(value==1);++f->originals;}
static void after(void*p){
 Fixture*f=p;assert(on_owner(p)&&f->originals>f->dispatches);
 ++f->dispatches;
 if(f->s.state==AU98_EMPTY&&f->prepared<0&&f->wired)
  (void)mu_bt_worker_poll(&f->worker,&f->prepared);
 f->rc=au98_drive(&f->s,&f->prepared,&f->policy,adopt,f,reclaim);
}
static void pulse(Fixture*f){mu_notify_call(&f->gate,original,f,1);}
static void sleep_ms(void){struct timespec t={0,10000000L};(void)nanosleep(&t,NULL);}
static void until_published(Fixture*f){
 for(int i=0;i<500&&f->s.state!=AU98_PUBLISHED;i++){pulse(f);sleep_ms();}
 assert(f->s.state==AU98_PUBLISHED&&f->prepared==-1&&f->s.fd>=0);
}
static int listener(void){
 int fd=socket(AF_INET,SOCK_STREAM,0),yes=1;struct sockaddr_in a={0};assert(fd>=0);
 assert(!setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes));
 a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(57342);
 assert(!bind(fd,(struct sockaddr*)&a,sizeof a)&&!listen(fd,4));return fd;
}
static void accept_and_exchange(Fixture*f){
 struct pollfd p={f->listener,POLLIN,0};assert(poll(&p,1,1000)==1);
 f->peer=accept(f->listener,NULL,NULL);assert(f->peer>=0);
 assert(send(f->peer,"M",1,0)==1);p.fd=f->s.fd;p.events=POLLIN;p.revents=0;
 assert(poll(&p,1,1000)==1);char c=0;assert(recv(f->s.fd,&c,1,0)==1&&c=='M');
}
static void check_closed(Fixture*f){
 struct pollfd p={f->peer,POLLIN,0};assert(poll(&p,1,1000)==1);
 char c;assert(recv(f->peer,&c,1,0)==0);close(f->peer);f->peer=-1;
}
int main(void){
 Fixture f;memset(&f,0,sizeof f);f.tid=pthread_self();f.prepared=f.peer=f.s.fd=-1;f.wired=1;
 f.hooks=(Au98Hooks){&f,yes,on_owner,yes,attach,tick,withdraw};
 f.owner.opaque=&f;f.owner.owner_ready=yes;f.owner.transport_ready=yes;
 f.owner.read_live=rd;f.owner.paired_authorized=auth;f.owner.iap2_profile_proven=profile;
 f.policy=(Au98DispatchPolicy){&f,on_owner,wired,barrier,released};
 f.gate=(MuNotifyGate){&f,on_owner,yes,after,0};
 f.listener=listener();assert(mu_bt_worker_start(&f.worker,57342)==0);
 until_published(&f);accept_and_exchange(&f);assert(f.s.generations==1&&f.attach==1);
 pulse(&f);assert(f.ticks==1);
 f.wired=0;pulse(&f);assert(f.s.state==AU98_DRAINING);
 pulse(&f);assert(f.s.state==AU98_RETIRED&&f.withdraws==1);check_closed(&f);
 pulse(&f);assert(f.s.state==AU98_RETIRED&&!f.reclaims&&f.rc==-1);
 f.release=1;pulse(&f);assert(f.s.state==AU98_EMPTY&&f.reclaims==1);
 f.wired=1;until_published(&f);accept_and_exchange(&f);assert(f.s.generations==2&&f.attach==2);
 f.wired=0;pulse(&f);pulse(&f);assert(f.s.state==AU98_RETIRED&&f.withdraws==2);check_closed(&f);
 pulse(&f);assert(f.s.state==AU98_EMPTY&&f.reclaims==2);
 assert(f.dispatches==f.originals&&!f.gate.inside);
 assert(mu_bt_worker_stop(&f.worker)==0);close(f.listener);
 puts("BT_OWNER_REAL_SOCKET_PTHREAD_TWO_SESSIONS_PASS_NO_OEM");return 0;
}
