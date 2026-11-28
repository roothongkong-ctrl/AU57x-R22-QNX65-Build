/* SPDX-License-Identifier: GPL-3.0-only */
#include "wcp_start.h"
#include <string.h>
#include <unistd.h>
int mu_wcp_start_begin(MuWcpStart*s,const MuBtNotifyBinding*n,const MuBtQueuePulseBinding*q,
 const char*path,char*const env[],int(*identity)(void*,const char*),void*ic,
 int(*ack)(void*,const char*),void*ac){
 char *child[128];
 unsigned i=0;
 if(!s||s->phase||s->daemon.pid>0||!n||!q||!path||!identity||!ack||
    !n->handoff||n->handoff->state!=AU98_EMPTY||n->worker||
    n->daemon_verified||n->ack_daemon_verified||n->daemon_context||
    !q->enter||!q->leave||!q->owner.read_live)return -1;
 /* Generate the exact inherited descriptor contract once, not a second
  * listener. Preserve the existing AP environment, reject ambiguity. */
 if(env)for(;env[i];++i){
  if(i>=125||!strncmp(env[i],"MU1005_BRIDGE_FD=",17)||
     !strncmp(env[i],"MU1005_BRIDGE_READY=",20))return -1;
  child[i]=env[i];
 }
 child[i++]="MU1005_BRIDGE_FD=3";
 child[i++]="MU1005_BRIDGE_READY=1";
 child[i]=0;
 s->notify=*n;s->queue=*q;s->fd=-1;s->notify.prepared_fd=&s->fd;
 int rc=mu_supervised_notify_prepare(&s->daemon,&s->notify,path,child,identity,ic,ack,ac);
 if(rc){s->phase=-1;return rc;}
 s->phase=1;
 return 0;
}
int mu_wcp_start_poll(MuWcpStart*s,int(*authorized)(void*),void*ctx){
 if(!s||!authorized)return -1;
 if(s->phase==3)return 1;
 if(s->phase!=1&&s->phase!=2)return -1;
 s->phase=2; /* no cancellation from supervisor after first queue attempt */
 int rc=mu_supervised_startup_tick(&s->startup,&s->daemon,&s->notify,
  &s->queue,0,500,1,authorized,ctx);
 if(rc==1)s->phase=3;
 else if(rc<0)s->phase=-1;
 return rc;
}
int mu_wcp_start_cancel_unpublished(MuWcpStart*s){
 if(!s||s->phase!=1||s->fd<0||
    !mu_daemon_supervisor_witness(&s->daemon,s->fd))return -1;
 /* No poll occurred: the descriptor has never been published to the AO.
  * Revoke our mailbox, close only the exact supervisor-owned endpoint. */
 s->daemon.transferred=0;s->fd=-1;s->phase=-1;
 return mu_daemon_supervisor_close(&s->daemon);
}
