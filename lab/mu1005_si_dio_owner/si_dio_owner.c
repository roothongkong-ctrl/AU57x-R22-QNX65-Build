/* SPDX-License-Identifier: GPL-3.0-only */
#include "si_dio_owner.h"
#include <string.h>
int mu_si_dio_begin(MuSiDioOwner*s,const MuSiDioOps*o,uint64_t epoch,
 char*const argv[],char*const env[],const char*lib){
 if(!s||!o||!o->admit||!o->cleanup||!epoch||epoch<=s->retired_epoch)return -1;
 /* Delegate actual native spawn, exact original argv and environment. */
 return mu_dio_launch_wireless(&s->launch,epoch,argv,env,lib,o->admit,o->ctx);
}
int mu_si_dio_retire(MuSiDioOwner*s,const MuSiDioOps*o,uint64_t epoch){
 if(!s||!o||!o->cleanup||!epoch||!s->launch.attempted||
    s->launch.epoch!=epoch||epoch<=s->retired_epoch)return -1;
 /* Never forget a live child or an unknown waitpid result. */
 if(s->launch.pid>0){
  int r=mu_dio_launch_reap(&s->launch);
  if(r!=0)return r==1?1:-1;
 }
 if(o->cleanup(o->ctx,epoch)!=1)return 1;
 s->retired_epoch=epoch;
 memset(&s->launch,0,sizeof s->launch);
 return 0;
}
