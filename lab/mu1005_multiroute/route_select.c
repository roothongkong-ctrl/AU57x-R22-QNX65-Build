/* SPDX-License-Identifier: GPL-3.0-only */
#include "route_select.h"
#include <string.h>
#define ALL (MU_ROUTE_DIRECT|MU_ROUTE_SI|MU_ROUTE_TYPE3)
static int single(unsigned r){return r==1u||r==2u||r==4u;}
void mu_route_init(MuRouteSelect*s,unsigned enabled,unsigned preferred){
 if(!s)return;
 memset(s,0,sizeof *s);s->enabled=enabled&ALL;
 s->preferred=single(preferred)&&(s->enabled&preferred)?preferred:0;
}
unsigned mu_route_begin(MuRouteSelect*s,uint64_t epoch,unsigned ready,int idle){
 if(!s||s->state!=MU_ROUTE_IDLE||idle!=1||!epoch||epoch<=s->last_epoch)return 0;
 unsigned remaining=s->enabled&~s->failed;
 /* Fixed original-first order even when a previous fallback succeeded.
  * Missing readiness is not a terminal failure: wait for a positive
  * capability verdict instead of skipping original SI on slow startup. */
 unsigned r=(remaining&MU_ROUTE_SI)?MU_ROUTE_SI:
  (remaining&MU_ROUTE_DIRECT)?MU_ROUTE_DIRECT:
  (remaining&MU_ROUTE_TYPE3)?MU_ROUTE_TYPE3:0;
 if(!r){s->state=MU_ROUTE_EXHAUSTED;return 0;}
 if(!(ready&r))return 0;
 s->active=r;s->epoch=s->last_epoch=epoch;s->state=MU_ROUTE_TRIAL;
 return r;
}
/* Only a certified no-side-effect incompatibility can skip an idle route. */
int mu_route_unavailable(MuRouteSelect*s,unsigned r,int definitive,int clean){
 if(!s||s->state!=MU_ROUTE_IDLE||s->active||!single(r)||!(s->enabled&r)||definitive!=1||clean!=1)return -1;
 s->failed|=r;if(s->preferred==r)s->preferred=0;
 if(!(s->enabled&~s->failed))s->state=MU_ROUTE_EXHAUSTED;
 return 0;
}
int mu_route_result(MuRouteSelect*s,unsigned r,uint64_t e,int result,int verified){
 if(!s||!single(r)||s->active!=r||s->epoch!=e||
   (s->state!=MU_ROUTE_TRIAL&&s->state!=MU_ROUTE_CONNECTED))return -1;
 if(result==0)return 0; /* pending, not failure */
 if(result==1){if(verified!=1)return -1;s->state=MU_ROUTE_CONNECTED;s->preferred=r;return 1;}
 if(result!=-1)return -1;
 s->failed|=r;if(s->preferred==r)s->preferred=0;
 s->state=MU_ROUTE_RESTORE;return 0;
}
int mu_route_usb(MuRouteSelect*s){
 if(!s)return -1;
 s->wired_pending=1;
 s->state=s->active?MU_ROUTE_RESTORE:MU_ROUTE_WIRED;
 return 0;
}
int mu_route_restored(MuRouteSelect*s,unsigned r,uint64_t e,int restored,int idle){
 if(!s||restored!=1||(idle!=0&&idle!=1))return -1;
 if(s->state==MU_ROUTE_WIRED&&!s->active&&r==0&&e==s->last_epoch&&idle==1){
  s->wired_pending=0;s->failed=0;s->state=MU_ROUTE_IDLE;return 1;
 }
 if(s->state!=MU_ROUTE_RESTORE||!single(r)||s->active!=r||s->epoch!=e)return -1;
 s->active=0;s->epoch=0;
 if(s->wired_pending||!idle){s->state=MU_ROUTE_WIRED;return 1;}
 s->state=(s->enabled&~s->failed)?MU_ROUTE_IDLE:MU_ROUTE_EXHAUSTED;
 return 1;
}
