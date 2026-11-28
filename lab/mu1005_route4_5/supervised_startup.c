/* SPDX-License-Identifier: GPL-3.0-only */
#include "supervised_startup.h"
int mu_supervised_startup_tick(MuSupervisedStartup*s,MuDaemonSupervisor*d,
 const MuBtNotifyBinding*n,const MuBtQueuePulseBinding*q,int(*trace_take)(void),
 unsigned total_ticks,unsigned trace_ticks,int(*startup)(void*),void*ctx){
 if(!s)return -1;
 if(s->phase==2)return 1; /* no reads of owner-side state after transfer */
 if(s->phase<0)return -1;
 if(!d||!n||!q||!startup||!total_ticks||total_ticks>10000u||
    !trace_ticks||trace_ticks>100u||!n->handoff||
    n->handoff->state!=AU98_EMPTY||!n->prepared_fd||n->worker||
    n->daemon_context!=d||n->daemon_verified!=mu_daemon_supervisor_witness||
    n->ack_daemon_verified!=mu_daemon_supervisor_witness||
    !q->enter||!q->leave||!q->owner.read_live||
    !mu_daemon_supervisor_witness(d,*n->prepared_fd)){
  s->phase=-1;return -1;
 }
 if(!s->phase){s->phase=1;s->remaining=total_ticks;}
 if(!s->remaining){s->phase=-1;return -1;}
 --s->remaining;
 int rc=mu_bt_startup_bootstrap_tick(&s->watch,trace_take,trace_ticks,q,n,startup,ctx);
 if(rc==1){s->phase=2;return 1;}
 if(rc!=0||!s->remaining){s->phase=-1;return -1;}
 return 0;
}
