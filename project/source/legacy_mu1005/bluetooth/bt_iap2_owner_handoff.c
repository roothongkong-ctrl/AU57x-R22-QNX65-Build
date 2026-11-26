/* SPDX-License-Identifier: GPL-3.0-only
 * The missing R97 -> native_binding.c owner handoff is explicit here.
 * Nothing calls this function automatically in the OEM process: the
 * original iAP2 owner/registration mode has not been identified in MU1005.
 * A registered BlueSDK service still requires genuine Apple iAP2/MFi data.
 */
#define _POSIX_C_SOURCE 200809L
#include "bt_iap2_owner_handoff.h"
#include "rf_arg_experiment.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

static int oem_ok(const Au98Handoff *s){
 const Au98Hooks *h=&s->hooks;
 return h->original_image_verified(h->opaque)==1 &&
        h->on_original_iap2_owner(h->opaque)==1 &&
        h->original_iap2_service_verified(h->opaque)==1 &&
        s->native_owner.owner_ready(s->native_owner.opaque)==1;
}
static int connected(unsigned port){
 struct sockaddr_in peer,actual;
 socklen_t len;
 struct pollfd waitfd;
 int fd,fl,rc,soerr=0;
 if(port<1 || port>65535)return -1;
 fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
 if((fl=fcntl(fd,F_GETFL,0))<0||fcntl(fd,F_SETFL,fl|O_NONBLOCK)<0)goto error;
 if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0)goto error;
 memset(&peer,0,sizeof peer);peer.sin_family=AF_INET;
 peer.sin_addr.s_addr=htonl(INADDR_LOOPBACK);peer.sin_port=htons((uint16_t)port);
 rc=connect(fd,(struct sockaddr *)&peer,sizeof peer);
 if(rc<0 && errno!=EINPROGRESS)goto error;
 if(rc<0){
  waitfd.fd=fd;waitfd.events=POLLOUT;waitfd.revents=0;
  if(poll(&waitfd,1,1000)!=1 || !(waitfd.revents&POLLOUT) ||
     (waitfd.revents&(POLLERR|POLLHUP|POLLNVAL)))goto error;
  len=sizeof soerr;
  if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&soerr,&len)||len!=sizeof soerr||soerr)goto error;
 }
 len=sizeof actual;
 if(getpeername(fd,(struct sockaddr *)&actual,&len)||len!=sizeof actual ||
    actual.sin_family!=AF_INET||actual.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
    actual.sin_port!=htons((uint16_t)port))goto error;
 len=sizeof actual;
 if(getsockname(fd,(struct sockaddr *)&actual,&len)||len!=sizeof actual ||
    actual.sin_family!=AF_INET||actual.sin_addr.s_addr!=htonl(INADDR_LOOPBACK))goto error;
 return fd;
error:
 (void)close(fd);return -1;
}
/* Network worker only, not the original SDK event thread. */
int au98_prepare_daemon_socket(unsigned port){
#if !defined(AU57X_R98_HOST_TEST) || AU57X_R98_HOST_TEST!=1
 if(port!=57342u)return -1;
#endif
 return connected(port);
}
int au98_publish(Au98Handoff*s,const Au98Hooks*h,const MuBtOwner*owner,unsigned port){
 unsigned char candidate_scn=0;
 int fd,rc;
 if(!s||!h||!owner||s->state!=AU98_EMPTY||
    !h->original_image_verified||!h->on_original_iap2_owner||
    !h->original_iap2_service_verified||!h->attach||!h->tick||!h->withdraw||
    !owner->owner_ready||!owner->transport_ready||!owner->read_live||
    !owner->paired_authorized||!owner->iap2_profile_proven)return -1;
#if !defined(AU57X_R98_HOST_TEST) || AU57X_R98_HOST_TEST!=1
 if(port!=57342u)return -1;
#endif
 if(h->original_image_verified(h->opaque)!=1 ||
    h->on_original_iap2_owner(h->opaque)!=1||
    h->original_iap2_service_verified(h->opaque)!=1||
    owner->owner_ready(owner->opaque)!=1||
    owner->transport_ready(owner->opaque)!=1||
    mu_rf_arg_choose(owner->iap2_profile_proven,owner->opaque,&candidate_scn)!=MU_RF_ARG_VERIFIED)return -1;
 fd=connected(port);if(fd<0)return -1;
 /* connect can block for up to one second. Recheck every owner claim
  * immediately before first native attach, not just before opening TCP.
  * A revoked transport/owner must never publish a callback to BlueSDK. */
 if(h->original_image_verified(h->opaque)!=1 ||
    h->on_original_iap2_owner(h->opaque)!=1 ||
    h->original_iap2_service_verified(h->opaque)!=1 ||
    owner->owner_ready(owner->opaque)!=1 ||
    owner->transport_ready(owner->opaque)!=1){
  (void)close(fd);return -1;
 }
 /* No callback/owner claim may be reused after this point, even after
  * failure. The native SDK may have published partial RF/SDP storage. */
 s->state=AU98_POISONED;s->fd=fd;s->hooks=*h;s->native_owner=*owner;
 /* One attested value for the complete owner lifetime, including the
  * original BlueSDK RF call. No re-reading a changing profile callback. */
 s->native_owner.rf_arg3_snapshot=candidate_scn;
 s->native_owner.rf_arg3_snapshot_valid=1u;
 rc=h->attach(h->opaque,&s->native_owner,fd);
 if(rc==0){s->state=AU98_PUBLISHED;++s->generations;return 0;}
 /* Never close/free callback-backed SDK storage on an ambiguous attach. */
 return -2;
}
/* Prepared socket handoff: no connect or blocking poll on OEM owner.
 * Caller retains fd on -1; on 0/-2 ownership transfers to handoff. */
static int socket_ready(int fd,unsigned port){
 struct sockaddr_in a; socklen_t len=sizeof a;int fl,err=0;
 struct pollfd p={fd,POLLIN,0};
 if(fd<0||!port||port>65535)return 0;
 fl=fcntl(fd,F_GETFL,0);if(fl<0||!(fl&O_NONBLOCK))return 0;
 fl=fcntl(fd,F_GETFD);if(fl<0||!(fl&FD_CLOEXEC))return 0;
 if(getpeername(fd,(struct sockaddr*)&a,&len)||len!=sizeof a||
    a.sin_family!=AF_INET||a.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
    a.sin_port!=htons((uint16_t)port))return 0;
 len=sizeof a;
 if(getsockname(fd,(struct sockaddr*)&a,&len)||len!=sizeof a||
    a.sin_family!=AF_INET||a.sin_addr.s_addr!=htonl(INADDR_LOOPBACK))return 0;
 len=sizeof err;
 if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&len)||len!=sizeof err||err)return 0;
 if(poll(&p,1,0)<0||(p.revents&(POLLERR|POLLHUP|POLLNVAL)))return 0;
 if(p.revents&POLLIN){
  unsigned char v;ssize_t r=recv(fd,&v,1,MSG_PEEK);
  if(r==0||(r<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK))return 0;
 }
 return 1;
}
int au98_adopt(Au98Handoff*s,const Au98Hooks*h,const MuBtOwner*o,
               int fd,unsigned port,int(*verified)(void*,int),void*ctx){
 uint8_t arg=0;
 if(!s||!h||!o||s->state!=AU98_EMPTY||s->generations==UINT32_MAX||
    !h->original_image_verified||!h->on_original_iap2_owner||
    !h->original_iap2_service_verified||!h->attach||!h->tick||!h->withdraw||
    !o->owner_ready||!o->transport_ready||!o->read_live||
    !o->paired_authorized||!o->iap2_profile_proven||!verified)return -1;
#if !defined(AU57X_R98_HOST_TEST) || AU57X_R98_HOST_TEST!=1
 if(port!=57342u)return -1;
#endif
 if(h->on_original_iap2_owner(h->opaque)!=1||h->original_image_verified(h->opaque)!=1||
    h->original_iap2_service_verified(h->opaque)!=1||o->owner_ready(o->opaque)!=1||
    o->transport_ready(o->opaque)!=1||!socket_ready(fd,port)||verified(ctx,fd)!=1||
    mu_rf_arg_choose(o->iap2_profile_proven,o->opaque,&arg)!=MU_RF_ARG_VERIFIED)return -1;
 if(!socket_ready(fd,port)||verified(ctx,fd)!=1||h->on_original_iap2_owner(h->opaque)!=1||
    h->original_image_verified(h->opaque)!=1||h->original_iap2_service_verified(h->opaque)!=1||
    o->owner_ready(o->opaque)!=1||o->transport_ready(o->opaque)!=1)return -1;
 s->state=AU98_POISONED;s->fd=fd;s->hooks=*h;s->native_owner=*o;
 s->native_owner.rf_arg3_snapshot=arg;s->native_owner.rf_arg3_snapshot_valid=1u;
 if(h->attach(h->opaque,&s->native_owner,fd)!=0)return -2;
 ++s->generations;s->state=AU98_PUBLISHED;return 0;
}
int au98_tick(Au98Handoff*s){
 if(!s||s->state!=AU98_PUBLISHED)return -1;
 /* A call from the wrong thread cannot mutate the serialized owner
  * state. On the original thread, however, loss of the actual image,
  * service, owner or authenticated transport is terminal for this
  * publication, even if the proof later becomes true again. */
 if(s->hooks.on_original_iap2_owner(s->hooks.opaque)!=1)return -1;
 if(oem_ok(s)&&s->native_owner.transport_ready(s->native_owner.opaque)==1&&
    s->hooks.tick(s->hooks.opaque)==0&&
    oem_ok(s)&&s->native_owner.transport_ready(s->native_owner.opaque)==1)
  return 0;
 /* A successfully published service can be quiesced after ordinary
  * transport failure. Do not treat this as an uncertain initial attach.
  * No more data-plane ticks or republish; only certified withdrawal. */
 s->state=AU98_DRAINING;return -2;
}
int au98_withdraw(Au98Handoff*s,int(*barrier)(void*,const Inbound*),void*ctx){
 int rc;
 if(!s||(s->state!=AU98_PUBLISHED&&s->state!=AU98_DRAINING)||!barrier||!oem_ok(s))return -1;
 rc=s->hooks.withdraw(s->hooks.opaque,barrier,ctx);
 /* -1 means drain pending or unregister preconditions not met.
  * Native disconnect may have been queued; never release the descriptor. */
 if(rc==-1){s->state=AU98_DRAINING;return -1;}
 if(rc!=0){s->state=AU98_POISONED;return -2;}
 /* QNX native owner confirms no live channel, no pending callback and the
  * peer socket is quiesced; only then is it valid to release the fd. */
 if(close(s->fd)!=0){s->state=AU98_POISONED;return -2;}
 s->fd=-1;s->state=AU98_RETIRED;return 0;
}

int au98_drive(Au98Handoff*s,int*fd,const Au98DispatchPolicy*p,Au98AdoptStep adopt,void*ctx,Au98ReclaimStep reclaim){
 if(!s||!fd||!p||!p->safe_dispatch||!p->wired_idle||
    !p->withdraw_barrier||!p->reclaimed||!adopt||!reclaim||
    p->safe_dispatch(p->opaque)!=1)return -1;
 if(s->state!=AU98_EMPTY&&s->state!=AU98_POISONED&&
    (!s->hooks.on_original_iap2_owner||
     s->hooks.on_original_iap2_owner(s->hooks.opaque)!=1))return -1;
 switch(s->state){
 case AU98_EMPTY:
  if(*fd<0||p->wired_idle(p->opaque)!=1)return 0;
  {int rc=adopt(ctx,s,*fd);
   if(rc==0||rc==-2)*fd=-1;
   return rc;}
 case AU98_PUBLISHED:
  if(p->wired_idle(p->opaque)!=1){s->state=AU98_DRAINING;return 0;}
  return au98_tick(s)==0?0:-1;
 case AU98_DRAINING:
  return au98_withdraw(s,p->withdraw_barrier,p->opaque);
 case AU98_RETIRED:
  return reclaim(s,p->reclaimed,p->opaque);
 case AU98_POISONED:
  return -2;
 default:return -1;
 }
}

/* Native execution path of the R98 -> R45 bridge. Coded as real calls to
 * native_binding.c, not a function-pointer-only host placeholder.
 * Strictly unavailable outside the pinned QNX ARM32 target. */
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
static int native_attach_adapter(void*unused,const MuBtOwner*o,int fd){
 (void)unused;return mu_bt_native_attach(o,fd);
}
static int native_tick_adapter(void*unused){
 (void)unused;return mu_bt_native_tick();
}
static int native_withdraw_adapter(void*unused,int(*barrier)(void*,const Inbound*),void*ctx){
 (void)unused;return mu_bt_native_withdraw(barrier,ctx);
}
int au98_bind_stock_native(Au98Handoff*s,const Au98OwnerProof*p,const MuBtOwner*o){
 /* Deprecated fail-closed entry point. au98_publish() performs a bounded
  * connect/poll and therefore MUST NOT be called from a BlueSDK/iAP2 owner
  * callback. The only native activation route is now:
  * worker prepares a nonblocking CLOEXEC fd -> au98_drive_stock_native()
  * adopts it on the proven owner thread. Keep this symbol for ABI/source
  * compatibility, but never block or mutate the OEM SDK from here. */
 (void)s;(void)p;(void)o;
 return -1;
}
int au98_adopt_stock_native(Au98Handoff*s,const Au98OwnerProof*p,const MuBtOwner*o,int fd,int(*verified)(void*,int),void*ctx){
 Au98Hooks h;
 if(!p||au100_route_select(p->route_evidence)!=AU100_GRAFT_RFCOMM_ROUTE)return -1;
 memset(&h,0,sizeof h);
 h.opaque=p->opaque;h.original_image_verified=p->original_image_verified;
 h.on_original_iap2_owner=p->on_original_iap2_owner;
 h.original_iap2_service_verified=p->original_iap2_service_verified;
 h.attach=native_attach_adapter;h.tick=native_tick_adapter;h.withdraw=native_withdraw_adapter;
 return au98_adopt(s,&h,o,fd,57342,verified,ctx);
}
int au98_rearm_stock_native(Au98Handoff*s,int(*reclaimed)(void*,const Inbound*),void*ctx){
 if(!s||s->state!=AU98_RETIRED||s->fd!=-1||s->generations==UINT32_MAX||
    !reclaimed||!oem_ok(s))return -1;
 if(mu_bt_native_reclaim(reclaimed,ctx)!=0)return -1;
 uint32_t generations=s->generations;
 memset(s,0,sizeof *s);s->generations=generations;s->fd=-1;
 return 0;
}
typedef struct {
 const Au98OwnerProof *proof;const MuBtOwner *owner;
 int (*verified)(void*,int);void *opaque;
} NativeDispatch;
static int adopt_dispatch(void*ctx,Au98Handoff*s,int fd){
 NativeDispatch*d=ctx;
 return au98_adopt_stock_native(s,d->proof,d->owner,fd,d->verified,d->opaque);
}
int au98_drive_stock_native(Au98Handoff*s,int*fd,const Au98DispatchPolicy*p,const Au98OwnerProof*proof,const MuBtOwner*o,int(*verified)(void*,int),void*ctx){
 NativeDispatch d={proof,o,verified,ctx};
 if(!proof||!o||!verified)return -1;
 return au98_drive(s,fd,p,adopt_dispatch,&d,au98_rearm_stock_native);
}
#else
int au98_bind_stock_native(Au98Handoff*s,const Au98OwnerProof*p,const MuBtOwner*o){
 (void)s;(void)p;(void)o;return -1;
}
int au98_adopt_stock_native(Au98Handoff*s,const Au98OwnerProof*p,const MuBtOwner*o,int fd,int(*v)(void*,int),void*ctx){(void)s;(void)p;(void)o;(void)fd;(void)v;(void)ctx;return -1;}
int au98_rearm_stock_native(Au98Handoff*s,int(*reclaimed)(void*,const Inbound*),void*ctx){(void)s;(void)reclaimed;(void)ctx;return -1;}
#endif

#if !defined(__QNXNTO__) || !defined(__arm__) || UINTPTR_MAX != UINT32_MAX
int au98_drive_stock_native(Au98Handoff*s,int*fd,const Au98DispatchPolicy*p,const Au98OwnerProof*proof,const MuBtOwner*o,int(*v)(void*,int),void*ctx){
 (void)s;(void)fd;(void)p;(void)proof;(void)o;(void)v;(void)ctx;return -1;
}
#endif
