/* SPDX-License-Identifier: GPL-3.0-only */
/* Coordinator fixture only: no OEM runtime or lease claim. */
#include "supervised_startup.h"
#include <assert.h>
#include <stdio.h>
static int live,rc,calls,checks;
int mu_daemon_supervisor_witness(void*p,int fd){assert(p);checks++;return live&&fd==8;}
int mu_bt_startup_bootstrap_tick(MuBtStartupWatch*w,int(*take)(void),unsigned ticks,
 const MuBtQueuePulseBinding*q,const MuBtNotifyBinding*n,int(*startup)(void*),void*ctx){
 assert(w&&ticks==3&&q&&n&&startup&&ctx);(void)take;calls++;return rc;
}
static int trace(void){return 1;}
static int enter(void*p){(void)p;return 1;}
static void leave(void*p){(void)p;}
static int readmem(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;return 0;}
static int auth(void*p){return p!=NULL;}
int main(void){
 MuDaemonSupervisor d={.fd=8};Au98Handoff h={0};int fd=8;
 MuBtNotifyBinding n={.handoff=&h,.prepared_fd=&fd,.daemon_context=&d,
 .daemon_verified=mu_daemon_supervisor_witness,.ack_daemon_verified=mu_daemon_supervisor_witness};
 MuBtQueuePulseBinding q={.owner={.read_live=readmem},.enter=enter,.leave=leave};
 MuSupervisedStartup s={0};live=1;
 #define T() mu_supervised_startup_tick(&s,&d,&n,&q,trace,4,3,auth,&d)
 assert(T()==0&&calls==1&&s.remaining==3);
 rc=1;assert(T()==1&&calls==2&&s.phase==2);
 int seen=checks;fd=-1;live=0;assert(T()==1&&calls==2&&checks==seen);
 s=(MuSupervisedStartup){0};fd=8;rc=0;assert(T()==-1&&calls==2);
 live=1;assert(T()==-1&&calls==2);
 s=(MuSupervisedStartup){0};for(int i=0;i<3;i++)assert(T()==0);
 assert(T()==-1&&calls==6&&s.phase==-1);assert(T()==-1&&calls==6);
 s=(MuSupervisedStartup){0};rc=-1;assert(T()==-1&&calls==7);
 rc=1;assert(T()==-1&&calls==7&&fd==8);
 s=(MuSupervisedStartup){0};rc=0;
 #define POLL() mu_supervised_startup_tick(&s,&d,&n,&q,NULL,4,3,auth,&d)
 assert(POLL()==0&&s.remaining==3);
 rc=1;assert(POLL()==1&&s.phase==2);
 s=(MuSupervisedStartup){0};rc=0;
 for(int i=0;i<3;i++)assert(POLL()==0);
 assert(POLL()==-1&&s.phase==-1);
 puts("MU1005_SUPERVISED_STARTUP_COORDINATOR_PASS_FIXTURE_ONLY");
 puts("MU1005_NO_DID_SUPERVISOR_DEADLINE_PASS_FIXTURE_ONLY");
 return 0;
}
