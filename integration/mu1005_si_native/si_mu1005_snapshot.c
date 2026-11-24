#include "si_mu1005_snapshot.h"
#include <string.h>
static int rd32(const struct mu_si_reader*r,uint32_t a,uint32_t*v){
 unsigned char b[4];
 if(r->read(r->context,a,b,sizeof(b))!=1)return 0;
 *v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);return 1;
}
static int once(const struct mu_si_reader*r,uint32_t c,struct mu_native_snapshot*s){
 uint32_t d=c+MU_SI_DP_OFF;
 return rd32(r,d+MU_NATIVE_APP_OFF,&s->app)&&rd32(r,d+MU_NATIVE_STATE_OFF,&s->state)&&
        rd32(r,d+MU_NATIVE_ID_OFF,&s->id)&&rd32(r,c+MU_SI_DEFERRED_APP_OFF,&s->deferred_app)&&
        rd32(r,c+MU_SI_DEFERRED_ID_OFF,&s->deferred_id);
}
int mu_si_snapshot(const struct mu_si_reader*r,uint32_t c,struct mu_native_snapshot*out){
 struct mu_native_snapshot a,b;
 if(!out)return MU_SNAPSHOT_UNSTABLE;
 memset(out,0,sizeof(*out));
 if(!r||!r->read||c<0x10000u||(c&3u)||c>UINT32_MAX-MU_SI_DEFERRED_ID_OFF-3u)return MU_SNAPSHOT_UNSTABLE;
 if(!once(r,c,&a)||!once(r,c,&b)||a.app!=b.app||a.state!=b.state||a.id!=b.id||a.deferred_app!=b.deferred_app||a.deferred_id!=b.deferred_id)return MU_SNAPSHOT_UNSTABLE;
 *out=a;return MU_SI_OK;
}
int mu_si_classify(const struct mu_native_snapshot*s,uint32_t expected){
 if(!s||!expected||expected==UINT32_MAX)return MU_SNAPSHOT_CONFLICT;
 const int deferred_empty=s->deferred_app==UINT32_MAX&&s->deferred_id==UINT32_MAX;
 const int deferred_ours=s->deferred_app==0&&s->deferred_id==expected;
 if(!deferred_empty&&!deferred_ours)return MU_SNAPSHOT_CONFLICT;
 if(s->app==UINT32_MAX&&s->state==0&&s->id==0)return deferred_ours?MU_SNAPSHOT_OWN_DEFERRED:MU_SNAPSHOT_IDLE;
 if(s->app==0&&s->id==expected&&s->state<=18)return MU_SNAPSHOT_OWN_CONTEXT;
 return MU_SNAPSHOT_CONFLICT;
}
int mu_si_activation_observe(const struct mu_si_reader*r,const struct mu_act_key*k,
 int (*proof)(void*,const struct mu_act_key*),void*ctx){
 struct mu_native_snapshot s,after;int cls,confirmed;
 if(!k||!k->epoch||!k->session||mu_si_snapshot(r,k->controller,&s)!=MU_SI_OK)return -1;
 cls=mu_si_classify(&s,k->id);
 if(cls==MU_SNAPSHOT_IDLE)return 0;
 if(cls==MU_SNAPSHOT_OWN_DEFERRED)return 1;
 if(cls!=MU_SNAPSHOT_OWN_CONTEXT)return -1;
 if(!proof)return 1;
 confirmed=proof(ctx,k);
 if(mu_si_snapshot(r,k->controller,&after)!=MU_SI_OK||
    s.app!=after.app||s.state!=after.state||s.id!=after.id||
    s.deferred_app!=after.deferred_app||s.deferred_id!=after.deferred_id)return -1;
 return confirmed==1?2:confirmed==0?1:-1;
}
