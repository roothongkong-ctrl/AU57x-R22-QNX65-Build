#include "si_mu1005_activation.h"
static int same(const struct mu_act_key*a,const struct mu_act_key*b){
 return a->controller==b->controller&&a->id==b->id&&a->session==b->session&&a->epoch==b->epoch;
}
/* Re-read after external callbacks: a matching ID alone does not prove
 * the same device record is still present. This is a snapshot check,
 * not a substitute for a native owner lease. */
static int record_current(const struct mu_act_ops*o,const struct mu_act_key*k,const struct mu_si_record_ref*r){
 struct mu_si_record_ref now;
 return mu_si_find_record(&o->reader,k->controller+MU_SI_DP_OFF,k->id,&now)==MU_SI_OK&&
        now.address==r->address&&o->admit(o->ctx,k)==1;
}
int mu_si_activate_once(struct mu_act_state*s,const struct mu_act_ops*o,const struct mu_act_key*k){
 struct mu_si_record_ref r;int v;
 if(!s||!o||!k||!k->controller||!k->id||!k->session||!k->epoch||!o->admit||!o->observe||!o->activate||k->controller>UINT32_MAX-MU_SI_DP_OFF)return MU_ACT_DENIED;
 if(s->phase&&(!same(&s->key,k)||s->phase==MU_ACT_UNCERTAIN))return MU_ACT_UNCERTAIN;
 if(o->admit(o->ctx,k)!=1||mu_si_find_record(&o->reader,k->controller+MU_SI_DP_OFF,k->id,&r)!=MU_SI_OK){
  if(s->phase){s->phase=MU_ACT_UNCERTAIN;return s->phase;}return MU_ACT_DENIED;
 }
 v=o->observe(o->ctx,k);
 if(v<0||v>2){if(s->phase)s->phase=MU_ACT_UNCERTAIN;return s->phase?s->phase:MU_ACT_DENIED;}
 if(!s->phase){
  if(v!=0||!record_current(o,k,&r))return MU_ACT_DENIED;
  s->key=*k;s->phase=MU_ACT_PENDING;
  o->activate((void*)(uintptr_t)k->controller,k->id,0u);
  if(o->admit(o->ctx,k)!=1){s->phase=MU_ACT_UNCERTAIN;return s->phase;}
  v=o->observe(o->ctx,k);
 }
 /* The observation can revoke the owner while it runs. Never publish a
  * connected state for a revoked session, including repeat observations. */
 if(!record_current(o,k,&r)){if(s->phase)s->phase=MU_ACT_UNCERTAIN;return s->phase?s->phase:MU_ACT_DENIED;}
 if(v==2){s->phase=MU_ACT_CONNECTED;return s->phase;}
 if(s->phase==MU_ACT_PENDING&&(v==0||v==1))return MU_ACT_PENDING;
 s->phase=MU_ACT_UNCERTAIN;return s->phase;
}
