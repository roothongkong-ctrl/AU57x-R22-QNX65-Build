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
enum { FL_IDLE=0, FL_PENDING=1, FL_DONE=2, FL_RETIRED=3 };
static int flight_state(MuInnerEventFlight *f){
 return __sync_val_compare_and_swap(&f->state,0,0);
}
static void event_completed(int status,const void *reply,void *ctx){
 MuInnerEventFlight *f=ctx;
 (void)reply;
 /* OEM must call once. Do not access session, CF objects, or sockets
  * from the HTTPClient queue. A retired mailbox stays allocated. */
 if(flight_state(f)!=FL_PENDING)return;
 f->status=status;
 (void)__sync_bool_compare_and_swap(&f->state,FL_PENDING,FL_DONE);
}
static int send_event(void *ctx,const unsigned char *p,size_t n){
 MuInnerSession *s=ctx;MuInnerEventFlight *f=s->flight;
 if(!f)return mu_inner_ap_send(&s->airplay,p,n);
 if(!__sync_bool_compare_and_swap(&f->state,FL_IDLE,FL_PENDING))return 0;
 if(mu_inner_ap_send_with_completion(&s->airplay,p,n,event_completed,f)!=1){
  /* Do not reuse storage on uncertain enqueue failure. */
  (void)__sync_lock_test_and_set(&f->state,FL_RETIRED);return -1;
 }
 return 1;
}
int mu_inner_session_track_events(MuInnerSession *s,MuInnerEventFlight *f){
 if(!s||!s->active||s->flight||!f||flight_state(f)!=FL_IDLE||
    s->socket.relay.tx_len||s->socket.relay.rx_len)return -1;
 s->flight=f;return 1;
}
int mu_inner_event_reclaim(MuInnerEventFlight *f,int drained){
 if(!f||drained!=1||flight_state(f)!=FL_RETIRED)return -1;
 f->status=0;(void)__sync_lock_test_and_set(&f->state,FL_IDLE);return 1;
}
void mu_inner_session_stop(MuInnerSession *s){
 if(!s)return;
 if(s->flight){(void)__sync_lock_test_and_set(&s->flight->state,FL_RETIRED);s->flight=NULL;}
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
 if(s->flight&&flight_state(s->flight)==FL_DONE){
  if(s->flight->status!=0){mu_inner_session_stop(s);return -1;}
  (void)__sync_lock_test_and_set(&s->flight->state,FL_IDLE);
 }
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

/* R105.035 / R105.037: actual original return VA 0x2cadc.
 * Other stock call at 0x2ccc4 returns 0x2ccc8 and is never intercepted. */
int mu_inner_session_dispatch_at(MuInnerSession *s,uint64_t e,MuInnerStockControl stock,
 uintptr_t bias,uintptr_t lr,void *session,unsigned flags,const void *command,
 const void *qualifier,const void *params,void **response){
 if(!stock)return -1;
 if(!bias||(bias&0xfffu)||bias>UINTPTR_MAX-0x2cadcu||lr!=bias+0x2cadcu)
  return stock(session,flags,command,qualifier,params,response);
 return mu_inner_session_dispatch_command(s,e,stock,session,flags,command,qualifier,params,response);
}
