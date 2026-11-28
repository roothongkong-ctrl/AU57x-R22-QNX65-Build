/* SPDX-License-Identifier: GPL-3.0-only */
#include "dio_existing_receiver.h"
#include <dlfcn.h>
static int admitted(const MuCprxEvidence*e,uint32_t gen,uint64_t epoch,uint32_t session){
 return e->admitted(e->opaque,gen)==1&&e->si_admitted(e->opaque,gen,epoch,session)==1;
}
static int same_owner(const MuDioGuard*g,void*h,void**slot,void*server,const void*manager){
 return g->verified_library(g->opaque,h)==1&&g->owner_exclusive(g->opaque,server,manager)==1&&
  g->verified_library(g->opaque,h)==1&&*slot==server;
}
int mu_dio_existing_receiver_open(MuCprxService*s,MuDioNative*n,void*h,
 const void*manager,uint32_t gen,const MuDioGuard*g,const char*orig,
 const char*target,const MuCprxEvidence*e,unsigned port,uint64_t epoch,
 uint32_t session,const char*dir,uid_t uid,gid_t gid){
 if(!s||!n||n->initialized||(s->backend.restore&&!s->backend_released)||
    !h||!manager||!gen||!g||!g->verified_library||!g->owner_exclusive||
    !e||!e->admitted||!e->identity||!e->media||!e->si_admitted||!e->si_client_verified||
    !port||port>65535||!dir||
    !epoch||!session||g->verified_library(g->opaque,h)!=1||
    !admitted(e,gen,epoch,session))return -1;
 /* Resolve only the original exported singleton from the exact handle.
  * The caller's existing lease excludes destruction throughout this call. */
 (void)dlerror();
 void **slot=(void**)dlsym(h,"gAirPlayReceiverServer");
 if(!slot||dlerror())return -1;
 void *server=*slot;
 if(!server||!same_owner(g,h,slot,server,manager)||!admitted(e,gen,epoch,session))return -1;
 if(mu_dio_bind_interface(n,h,server,manager,gen,g,orig,target)!=0)return -1;
 if(same_owner(g,h,slot,server,manager)&&admitted(e,gen,epoch,session)&&
    mu_cprx_open_si_unix(s,n,e,port,epoch,session,dir,uid,gid)==0){
  if(same_owner(g,h,slot,server,manager)&&admitted(e,gen,epoch,session))return 0;
  /* A published CPRX listener owns its backend: close through CPRX, never
   * bypass its socket-path lease or independently restore the receiver. */
  return mu_cprx_shutdown(s)==1?-1:-2;
 }
 /* Never discard a bound receiver when the original owner is revoked. */
 return mu_dio_restore(n)==0?-1:-2;
}
