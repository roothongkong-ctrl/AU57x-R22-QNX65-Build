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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
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
 fd_set wfds;
 struct timeval deadline;
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
  FD_ZERO(&wfds);FD_SET(fd,&wfds);deadline.tv_sec=1;deadline.tv_usec=0;
  if(select(fd+1,NULL,&wfds,NULL,&deadline)!=1)goto error;
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
int au98_tick(Au98Handoff*s){
 if(!s||s->state!=AU98_PUBLISHED||!oem_ok(s))return -1;
 if(s->hooks.tick(s->hooks.opaque)==0)return 0;
 s->state=AU98_POISONED;return -2;
}
int au98_withdraw(Au98Handoff*s,int(*barrier)(void*,const Inbound*),void*ctx){
 int rc;
 if(!s||s->state!=AU98_PUBLISHED||!barrier||!oem_ok(s))return -1;
 rc=s->hooks.withdraw(s->hooks.opaque,barrier,ctx);
 if(rc!=0){s->state=AU98_POISONED;return -2;}
 /* QNX native owner confirms no live channel, no pending callback and the
  * peer socket is quiesced; only then is it valid to release the fd. */
 if(close(s->fd)!=0){s->state=AU98_POISONED;return -2;}
 s->fd=-1;s->state=AU98_RETIRED;return 0;
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
 Au98Hooks h;
 if(!p||au100_route_select(p->route_evidence)!=AU100_GRAFT_RFCOMM_ROUTE||
    !p->original_image_verified||!p->on_original_iap2_owner||
    !p->original_iap2_service_verified)return -1;
 memset(&h,0,sizeof h);
 h.opaque=p->opaque;h.original_image_verified=p->original_image_verified;
 h.on_original_iap2_owner=p->on_original_iap2_owner;
 h.original_iap2_service_verified=p->original_iap2_service_verified;
 h.attach=native_attach_adapter;h.tick=native_tick_adapter;
 h.withdraw=native_withdraw_adapter;
 return au98_publish(s,&h,o,57342);
}
#else
int au98_bind_stock_native(Au98Handoff*s,const Au98OwnerProof*p,const MuBtOwner*o){
 (void)s;(void)p;(void)o;return -1;
}
#endif
