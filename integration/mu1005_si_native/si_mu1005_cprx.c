#include "si_mu1005_cprx.h"
#include <string.h>
#define QUERY_GAP_MS 100u
#define INITIAL_WINDOW_MS 20000u
static int key_same(const struct mu_owner_session*a,const struct mu_owner_session*b){return a->controller==b->controller&&a->epoch==b->epoch&&a->session==b->session;}
static int admitted(void*p,const struct mu_owner_session*k){
 struct mu_si_cprx*b=p;const MuCprxRemoteEndpoint*e=&b->endpoint;
 return key_same(k,&b->key)&&b->native.admitted(b->native.context,k)==1&&
 b->native.registration.owner_session_current(b->native.registration.context,k->controller+MU_SI_DP_OFF,k->epoch,k->session)==1&&
 e->si_owner_verified(e->owner_ctx,e->generation,e->epoch,e->session)==1;
}
static int receiver(void*p,const struct mu_act_key*k){
 struct mu_si_cprx*b=p;
 if(k->controller!=b->key.controller||k->epoch!=b->key.epoch||k->session!=b->key.session||k->id!=b->owner.id||!admitted(b,&b->key))return -1;
 return b->proof;
}
static int cleanup(void*p,const struct mu_owner_session*k,uint32_t id){struct mu_si_cprx*b=p;return key_same(k,&b->key)&&b->native.cleanup_confirmed(b->native.context,k,id)==1;}
static struct mu_owner_ops wrapped(struct mu_si_cprx*b){struct mu_owner_ops o=b->native;o.context=b;o.admitted=admitted;o.receiver_confirmed=receiver;o.cleanup_confirmed=cleanup;return o;}
static void close_query(struct mu_si_cprx*b){mu_cprx_client_close(&b->query);mu_cprx_client_init(&b->query);b->querying=0;b->proof=0;}
static int fail(struct mu_si_cprx*b){close_query(b);if(b->owner.phase){b->owner.phase=MU_OWNER_UNCERTAIN;return MU_OWNER_UNCERTAIN;}return MU_OWNER_DENIED;}
int mu_si_cprx_init(struct mu_si_cprx*b,const struct mu_owner_ops*o,const struct mu_owner_session*k,const MuCprxRemoteEndpoint*e){
 if(!b||!o||!k||!e||!k->session||!k->epoch||k->controller<0x10000u||(k->controller&3u)||k->controller>UINT32_MAX-MU_SI_DEFERRED_ID_OFF-3u||
 !e->generation||!e->airplay_port||e->airplay_port>65535u||e->epoch!=k->epoch||e->session!=k->session||!e->si_owner_verified||!e->dio_server_verified||e->socket_path[0]!='/'||!memchr(e->socket_path,0,sizeof e->socket_path)||
 !o->admitted||!o->registration.owner_session_current||!o->cleanup_confirmed||!o->activate)return MU_OWNER_DENIED;
 if(b->initialized)return MU_OWNER_DENIED;
 memset(b,0,sizeof *b);b->native=*o;b->key=*k;b->endpoint=*e;mu_cprx_client_init(&b->query);b->initialized=1;return 0;
}
int mu_si_cprx_tick(struct mu_si_cprx*b,uint64_t now,void*key,void*info,size_t size){
 int rc;struct mu_owner_ops o;
 if(!b||!b->initialized)return MU_OWNER_DENIED;
 if(b->owner.phase==MU_OWNER_UNCERTAIN)return MU_OWNER_UNCERTAIN;
 if(!admitted(b,&b->key)||(b->clock_set&&now<b->last_ms)||now>UINT64_MAX-INITIAL_WINDOW_MS)return fail(b);
 if(!b->clock_set){b->clock_set=1;b->first_ms=now;}
 b->last_ms=now;o=wrapped(b);
 if(!b->owner.phase){
  rc=mu_owner_tick(&b->owner,&o,&b->key,key,info,size);
  if(rc!=MU_OWNER_PENDING)return rc;
  b->first_ms=now;return MU_OWNER_PENDING;
 }
 if(b->owner.phase==MU_OWNER_PENDING&&now-b->first_ms>=INITIAL_WINDOW_MS)return fail(b);
 if(!b->querying){
  if(now<b->next_ms)return MU_OWNER_PENDING;
  if(mu_cprx_client_begin_remote(&b->query,&b->endpoint,MU_CPRX_OP_SESSION_STATUS,now)<0)return fail(b);
  b->querying=1;
 }
 rc=mu_cprx_client_tick(&b->query,now);
 if(rc==MU_CPRX_CLIENT_PENDING)return MU_OWNER_PENDING;
 if(rc==MU_CPRX_CLIENT_BUSY){close_query(b);b->next_ms=now+QUERY_GAP_MS;return b->owner.phase==MU_OWNER_CONNECTED?fail(b):MU_OWNER_PENDING;}
 if(rc!=MU_CPRX_CLIENT_CONFIRMED||b->query.operation!=MU_CPRX_OP_SESSION_STATUS||!b->query.remote_mode||b->query.generation!=b->endpoint.generation||b->query.epoch!=b->key.epoch||b->query.session!=b->key.session)return fail(b);
 if(b->query.observed_media!=0&&b->query.observed_media!=4&&b->query.observed_media!=5&&b->query.observed_media!=7)return fail(b);
 if(b->query.observed_inner<0||b->query.observed_inner>1||b->query.observed_channels>15||((b->query.observed_media!=7||!b->query.observed_inner)&&b->query.observed_channels))return fail(b);
 b->proof=b->query.observed_media==7&&b->query.observed_inner==1&&b->query.observed_channels==15;
 rc=mu_owner_tick(&b->owner,&o,&b->key,NULL,NULL,0);
 close_query(b);b->next_ms=now+QUERY_GAP_MS;
 if(rc==MU_OWNER_CONNECTED||rc==MU_OWNER_PENDING)return rc;
 return fail(b);
}
int mu_si_cprx_cleanup(struct mu_si_cprx*b){
 if(!b||!b->initialized)return MU_OWNER_DENIED;
 close_query(b);
 if(b->owner.phase){struct mu_owner_ops o=wrapped(b);int rc=mu_owner_clear_after_cleanup(&b->owner,&o,&b->key);if(rc!=0)return rc;}
 memset(b,0,sizeof *b);return 0;
}
