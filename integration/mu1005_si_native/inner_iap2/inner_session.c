/* SPDX-License-Identifier: GPL-3.0-only */
#include "inner_session.h"
#include <string.h>
static int ap_live(MuInnerSession *s,uint64_t e){
 MuInnerAirplay *a=&s->airplay;
 return a->bound&&a->epoch==e&&a->proof.library(a->proof.ctx,a->handle)==1&&
        a->proof.owner(a->proof.ctx,a->session,e)==1;
}
static int peer_live(void *ctx,int fd,uint64_t e){
 MuInnerSession *s=ctx;
 return ap_live(s,e)&&s->peer&&s->peer(s->ctx,fd,e)==1;
}
static int send_event(void *ctx,const unsigned char *p,size_t n){
 MuInnerSession *s=ctx;
 return mu_inner_ap_send(&s->airplay,p,n);
}
void mu_inner_session_stop(MuInnerSession *s){
 if(!s)return;
 mu_inner_socket_close(&s->socket);
 mu_inner_ap_unbind(&s->airplay);
 s->active=0;s->ctx=NULL;s->peer=NULL;
}
int mu_inner_session_start(MuInnerSession *s,void *h,void *session,uint64_t e,
 const MuInnerAirplayProof *proof,int fd,void *ctx,int (*peer)(void *,int,uint64_t)){
 if(!s||s->active||s->airplay.bound||s->socket.verified||!peer||fd<0||!e||e<=s->last_epoch)return -1;
 if(mu_inner_ap_bind(&s->airplay,h,session,e,proof)!=1)return -1;
 s->ctx=ctx;s->peer=peer;
 if(mu_inner_socket_adopt(&s->socket,fd,e,s,peer_live,send_event)!=1){
  mu_inner_session_stop(s);return -1;
 }
 s->last_epoch=e;s->active=1;return 1;
}
int mu_inner_session_tick(MuInnerSession *s,uint64_t e){
 if(!s||!s->active||e!=s->airplay.epoch)return -1;
 if(!ap_live(s,e)||mu_inner_socket_tick(&s->socket,e)!=1){
  mu_inner_session_stop(s);return -1;
 }
 return 1;
}
int mu_inner_session_control(MuInnerSession *s,uint64_t e,const void *session,
 const void *command,const void *params){
 if(!s||!s->active||e!=s->airplay.epoch||session!=s->airplay.session)return -1;
 if(!peer_live(s,s->socket.fd,e)){mu_inner_session_stop(s);return -1;}
 int r=mu_inner_ap_control_parts(&s->airplay,&s->socket.relay,e,session,command,params);
 if(r<0)mu_inner_session_stop(s);
 return r;
}

/* Adapter for the exact six-argument SessionControl call in the original
 * _requestProcessCommand. The installer must independently establish that
 * callsite and original queue; this code does not patch a library. */
int mu_inner_session_http_command(MuInnerSession *s,uint64_t e,void *session,
 unsigned flags,const void *command,const void *qualifier,const void *params,
 void **response,int *status){
 if(!status)return 0;
 if(!s||!s->active||session!=s->airplay.session||flags!=1||qualifier||!response)
  return 0;
 MuInnerAirplay *a=&s->airplay;
 if(!ap_live(s,e)){*status=-1;return 1;}
 if(!command||a->type(command)!=a->string_type()||!a->equal(command,a->cmd))
  return 0;
 /* Original _requestProcessCommand initializes response to NULL and makes
  * the empty dictionary itself on successful return. Never overwrite an
  * existing object or fabricate a successful response for backpressure. */
 if(*response){*status=-1;return 1;}
 int r=mu_inner_session_control(s,e,session,command,params);
 *status=r==1?0:-1;
 return 1;
}

/* Preserve every argument and the original return code on the stock path.
 * The caller supplies the verified nonrecursive original target. */
int mu_inner_session_dispatch_command(MuInnerSession *s,uint64_t e,MuInnerStockControl stock,
 void *session,unsigned flags,const void *command,const void *qualifier,const void *params,void **response){
 int status=-1;
 if(!stock)return -1;
 if(mu_inner_session_http_command(s,e,session,flags,command,qualifier,params,response,&status))
  return status;
 return stock(session,flags,command,qualifier,params,response);
}
