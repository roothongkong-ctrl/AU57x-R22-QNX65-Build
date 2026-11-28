/* SPDX-License-Identifier: GPL-3.0-only */
#include "dio_existing_receiver.h"
#include <dlfcn.h>
int mu_dio_existing_receiver_open(MuCprxService*s,MuDioNative*n,void*h,
 const void*manager,uint32_t gen,const MuDioGuard*g,const char*orig,
 const char*target,const MuCprxEvidence*e,unsigned port,uint64_t epoch,
 uint32_t session,const char*dir,uid_t uid,gid_t gid){
 if(!s||!n||n->initialized||(s->backend.restore&&!s->backend_released)||
    !h||!manager||!gen||!g||!g->verified_library||!g->owner_exclusive||
    !e||!e->admitted||!e->si_admitted||!e->si_client_verified||
    !epoch||!session||g->verified_library(g->opaque,h)!=1||
    e->admitted(e->opaque,gen)!=1||
    e->si_admitted(e->opaque,gen,epoch,session)!=1)return -1;
 /* Resolve only the original exported singleton from the exact handle.
  * The caller's existing lease excludes destruction throughout this call. */
 (void)dlerror();
 void **slot=(void**)dlsym(h,"gAirPlayReceiverServer");
 if(!slot||dlerror())return -1;
 void *server=*slot;
 if(!server||g->owner_exclusive(g->opaque,server,manager)!=1||
    *slot!=server||g->verified_library(g->opaque,h)!=1)return -1;
 if(mu_dio_bind_interface(n,h,server,manager,gen,g,orig,target)!=0)return -1;
 if(g->owner_exclusive(g->opaque,server,manager)==1&&*slot==server&&
    g->verified_library(g->opaque,h)==1&&
    mu_cprx_open_si_unix(s,n,e,port,epoch,session,dir,uid,gid)==0)return 0;
 /* Never discard a bound receiver when the original owner is revoked. */
 return mu_dio_restore(n)==0?-1:-2;
}
