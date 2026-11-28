/* Coordinator fixtures only, native startup evidence is not simulated into production. */
#include "wcp_start.h"
#include "bt_startup_snapshot.h"
#include <assert.h>
#include <stdio.h>
static int auth,started,lease,held,spawns,retracts,revoke_after_spawn;
static int authorized(void*p){(void)p;return auth;}
static int enter(void*p){(void)p;if(!lease)return 0;assert(!held);held=1;return 1;}
static void leave(void*p){(void)p;assert(held);held=0;}
static int read_live(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;assert(held);return 0;}
int mu_bt_sdk_lifecycle_sites(int(*r)(void*,uint32_t,void*,size_t),void*p){(void)r;(void)p;assert(held);return started;}
uint32_t mu_bt_sdk_service_address(int(*r)(void*,uint32_t,void*,size_t),void*p){(void)r;(void)p;assert(held);return started?0x1234:0;}
static int identity(void*p,const char*s){(void)p;(void)s;return 1;}
int mu_supervised_notify_prepare(MuDaemonSupervisor*d,MuBtNotifyBinding*n,const char*p,char*const e[],int(*i)(void*,const char*),void*ic,int(*a)(void*,const char*),void*ac){
 (void)p;(void)e;(void)i;(void)ic;(void)a;(void)ac;assert(!held);spawns++;d->pid=99;d->fd=8;d->transferred=1;*n->prepared_fd=8;if(revoke_after_spawn)started=0;return 0;
}
int mu_daemon_supervisor_retract_unpublished(MuDaemonSupervisor*d,int f){assert(f==8&&d->transferred);retracts++;d->transferred=0;d->fd=-1;return 0;}
int mu_supervised_startup_tick(MuSupervisedStartup*s,MuDaemonSupervisor*d,const MuBtNotifyBinding*n,const MuBtQueuePulseBinding*q,int(*t)(void),unsigned a,unsigned b,int(*fn)(void*),void*c){
 (void)s;(void)d;(void)n;(void)q;(void)t;(void)a;(void)b;(void)fn;(void)c;return 1;
}
int main(void){
 MuWcpStart s={0};Au98Handoff h={0};
 MuBtNotifyBinding n={.handoff=&h};
 MuBtQueuePulseBinding q={.owner={.read_live=read_live},.enter=enter,.leave=leave};
 char*env[]={0};
 #define B() mu_wcp_start_begin(&s,&n,&q,"/donor",env,identity,0,identity,0,authorized,0)
 assert(B()==MU_WCP_START_WAIT&&!spawns);
 auth=1;assert(B()==MU_WCP_START_WAIT&&!spawns);
 lease=1;assert(B()==MU_WCP_START_WAIT&&!spawns);
 started=1;revoke_after_spawn=1;assert(B()==-1&&spawns==1&&retracts==1&&s.fd==-1);
 s=(MuWcpStart){0};started=1;revoke_after_spawn=0;
 assert(B()==0&&spawns==2&&s.phase==1);
 assert(mu_wcp_start_cancel_unpublished(&s)==0&&retracts==2&&s.fd==-1);
 s=(MuWcpStart){0};assert(B()==0);
 assert(mu_wcp_start_poll(&s,authorized,0)==1);
 assert(mu_wcp_start_cancel_unpublished(&s)==-1&&retracts==2);
 puts("ORIGINAL_STARTUP_BEFORE_DONOR_AND_UNPUBLISHED_RETRACTION_PASS_FIXTURE_ONLY");
}
