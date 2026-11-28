/* SPDX-License-Identifier: GPL-3.0-only */
#include "supervised_notify.h"
#include <string.h>
static int require_ready(char*const env[]){
 unsigned seen=0;if(!env)return 0;
 for(unsigned i=0;env[i];i++){
  if(i>=127)return 0;
  if(!strncmp(env[i],"MU1005_BRIDGE_READY=",20)){
   if(strcmp(env[i],"MU1005_BRIDGE_READY=1")||++seen!=1)return 0;
  }
 }
 return seen==1;
}
typedef struct {
 int (*identity)(void*,const char*);void *identity_ctx;
 int (*ack)(void*,const char*);void *ack_ctx;
} MuDaemonProof;
static int prove(void *p,const char *path){
 MuDaemonProof *v=p;
 return v->identity(v->identity_ctx,path)==1&&v->ack(v->ack_ctx,path)==1;
}
int mu_supervised_notify_prepare(MuDaemonSupervisor*s,MuBtNotifyBinding*b,
 const char*path,char*const env[],int(*identity)(void*,const char*),void*identity_ctx,
 int(*ack)(void*,const char*),void*ack_ctx){
 if(!s||!b||!b->handoff||b->handoff->state!=AU98_EMPTY||
    !b->prepared_fd||*b->prepared_fd>=0||b->worker||
    b->daemon_verified||b->ack_daemon_verified||b->daemon_context||
    !identity||!ack||!require_ready(env))return -1;
 MuDaemonProof proof={identity,identity_ctx,ack,ack_ctx};
 if(mu_daemon_supervisor_start(s,path,env,prove,&proof)!=0)return s->pid>0?-2:-1;
 /* No second TCP connect: transfer the supervisor's existing exact
  * connected endpoint. The owner consumes it once through au98_adopt. */
 if(mu_daemon_supervisor_offer(s,b->prepared_fd)!=0){
  (void)mu_daemon_supervisor_close(s);
  return -2; /* supervisor may still need to reap; no retry over it */
 }
 b->daemon_context=s;
 b->daemon_verified=mu_daemon_supervisor_witness;
 b->ack_daemon_verified=mu_daemon_supervisor_witness;
 return 0;
}
