/* SPDX-License-Identifier: GPL-3.0-only */
#include "wcp_start.h"
#include "bt_startup_snapshot.h"
#include <string.h>
#include <unistd.h>
static int begin_prepared(MuWcpStart*s,const MuBtNotifyBinding*n,const MuBtQueuePulseBinding*q,
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
/* Real original initialized service, not a delay or DID trace.
 * The same lifetime lease is needed during the original read. */
static int stock_ready(const MuBtQueuePulseBinding*q,int(*authorized)(void*),void*ctx){
 if(!q||!q->enter||!q->leave||!q->owner.read_live||!authorized)return -1;
 int a=authorized(ctx);if(a!=1)return a==0?0:-1;
 int entered=q->enter(q->lease_context);if(entered!=1)return entered==0?0:-1;
 const MuBtOwner*o=&q->owner;
 uint32_t service=0;
 if(mu_bt_sdk_lifecycle_sites(o->read_live,o->opaque))
  service=mu_bt_sdk_service_address(o->read_live,o->opaque);
 q->leave(q->lease_context);
 if(!service||(o->services_address&&service!=o->services_address))return 0;
 a=authorized(ctx);return a==1?1:(a==0?0:-1);
}
int mu_wcp_start_begin(MuWcpStart*s,const MuBtNotifyBinding*n,const MuBtQueuePulseBinding*q,
 const char*path,char*const env[],int(*identity)(void*,const char*),void*ic,
 int(*ack)(void*,const char*),void*ac,int(*authorized)(void*),void*ctx){
 if(!s||s->phase||s->daemon.pid>0)return -1;
 int r=stock_ready(q,authorized,ctx);
 if(r!=1)return r==0?MU_WCP_START_WAIT:-1;
 r=begin_prepared(s,n,q,path,env,identity,ic,ack,ac);
 if(r!=0)return r;
 /* READY wait may have outlived the original service. No publication
  * has taken place yet; retract the mailbox and reap safely. */
 if(stock_ready(q,authorized,ctx)!=1){
  (void)mu_wcp_start_cancel_unpublished(s);return -1;
 }
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
 if(!s||(s->phase!=1&&s->phase!=-2)||s->fd<0)return -1;
 /* No poll occurred. Supervisor identity check does not require the
  * child to be alive; close its exact endpoint even after child exit. */
 int rc=mu_daemon_supervisor_retract_unpublished(&s->daemon,s->fd);
 if(rc<0){s->phase=-2;return rc;}
 s->fd=-1;s->phase=-1;return rc;
}
