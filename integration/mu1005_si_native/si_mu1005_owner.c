#include "si_mu1005_owner.h"
#include <string.h>
struct bridge {const struct mu_owner_ops *ops; const struct mu_owner_session *key;};
static int same(const struct mu_owner_session*a,const struct mu_owner_session*b){return a->controller==b->controller&&a->session==b->session&&a->epoch==b->epoch;}
static int valid(const struct mu_owner_session*k){return k&&k->controller>=0x10000u&&!(k->controller&3u)&&k->controller<=UINT32_MAX-MU_SI_DEFERRED_ID_OFF-3u&&k->session&&k->epoch;}
static int uncertain(struct mu_owner_state*s){s->phase=MU_OWNER_UNCERTAIN;return s->phase;}
static int admitted(const struct bridge*b){return b->ops->admitted(b->ops->context,b->key)==1;}
static int reg_admit(void*c,uint32_t p,uint64_t e,uint32_t s){struct bridge*b=c;const struct mu_reg_ops*r=&b->ops->registration;return p==b->key->controller+MU_SI_DP_OFF&&e==b->key->epoch&&s==b->key->session&&admitted(b)&&r->owner_session_current(r->context,p,e,s)==1;}
static int act_admit(void*c,const struct mu_act_key*k){struct bridge*b=c;return k->controller==b->key->controller&&reg_admit(c,k->controller+MU_SI_DP_OFF,k->epoch,k->session);}
static int observe(void*c,const struct mu_act_key*k){struct bridge*b=c;return mu_si_activation_observe(&b->ops->registration.reader,k,b->ops->receiver_confirmed,b->ops->context);}
static int idle(const struct mu_owner_ops*o,uint32_t c){struct mu_native_snapshot s;return mu_si_snapshot(&o->registration.reader,c,&s)==MU_SI_OK&&s.app==UINT32_MAX&&s.state==0&&s.id==0&&s.deferred_app==UINT32_MAX&&s.deferred_id==UINT32_MAX;}
int mu_owner_tick(struct mu_owner_state*s,const struct mu_owner_ops*o,const struct mu_owner_session*k,void*key,void*info,size_t size){
 int rc;struct bridge b={o,k};
 if(!s||!o||!valid(k)||!o->admitted||!o->activate||!o->receiver_confirmed||!o->registration.owner_session_current)return MU_OWNER_DENIED;
 if(s->phase&&(!same(&s->owner,k)||s->phase==MU_OWNER_UNCERTAIN))return MU_OWNER_UNCERTAIN;
 if(!reg_admit(&b,k->controller+MU_SI_DP_OFF,k->epoch,k->session))return s->phase?uncertain(s):MU_OWNER_DENIED;
 if(!s->phase){
  struct mu_reg_ops r=o->registration;
  if(!idle(o,k->controller))return MU_OWNER_DENIED;
  r.context=&b;r.owner_session_current=reg_admit;
  s->owner=*k;s->phase=MU_OWNER_UNCERTAIN;
  rc=mu_si_register_prepared(&r,k->controller+MU_SI_DP_OFF,k->epoch,k->session,key,info,size,&s->id);
  if(rc==MU_REG_DENIED){memset(s,0,sizeof(*s));return MU_OWNER_DENIED;}
  if(rc!=MU_SI_OK)return MU_OWNER_UNCERTAIN;
  s->phase=MU_OWNER_PENDING;
 }
 struct mu_act_key ak={k->controller,s->id,k->session,k->epoch};
 struct mu_act_ops a={.ctx=&b,.reader=o->registration.reader,.admit=act_admit,.observe=observe,.activate=o->activate};
 rc=mu_si_activate_once(&s->activation,&a,&ak);
 if(!act_admit(&b,&ak))return uncertain(s);
 if(rc==MU_ACT_PENDING){s->phase=MU_OWNER_PENDING;return s->phase;}
 if(rc==MU_ACT_CONNECTED){s->phase=MU_OWNER_CONNECTED;return s->phase;}
 return uncertain(s);
}
int mu_owner_clear_after_cleanup(struct mu_owner_state*s,const struct mu_owner_ops*o,const struct mu_owner_session*k){
 if(!s||!o||!valid(k)||!s->phase||!same(&s->owner,k)||!o->cleanup_confirmed)return MU_OWNER_DENIED;
 s->phase=MU_OWNER_UNCERTAIN;
 if(!idle(o,k->controller)||o->cleanup_confirmed(o->context,k,s->id)!=1||!idle(o,k->controller))return MU_OWNER_UNCERTAIN;
 memset(s,0,sizeof(*s));return MU_SI_OK;
}
