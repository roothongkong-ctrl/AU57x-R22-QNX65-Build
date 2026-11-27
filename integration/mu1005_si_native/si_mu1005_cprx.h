#ifndef MU1005_SI_CPRX_H
#define MU1005_SI_CPRX_H
#include "si_mu1005_owner.h"
#include "cprx_service.h"
/* Requires the independently verified OEM owner and endpoint. No install. */
struct mu_si_cprx {
 struct mu_owner_state owner;
 struct mu_owner_ops native;
 struct mu_owner_session key;
 MuCprxRemoteEndpoint endpoint;
 MuCprxClient query;
 uint64_t last_ms,first_ms,next_ms;
 int initialized,clock_set,querying,proof;
 unsigned required_channels; /* 0 session, 1 first video, 15 complete media */
 int first_session_seen; /* historical observation, NEVER a live grant */
};
int mu_si_cprx_init(struct mu_si_cprx*,const struct mu_owner_ops*,const struct mu_owner_session*,const MuCprxRemoteEndpoint*);
/* Explicit first-connect mode: authenticated original receiver + actual
 * video evidence, without requiring audio/MMI/steering. Does not prove LCD. */
/* Authenticated same-session active receiver, no media channel prerequisite. */
int mu_si_cprx_init_first_session(struct mu_si_cprx*,const struct mu_owner_ops*,const struct mu_owner_session*,const MuCprxRemoteEndpoint*);
/* Retained through disconnect until cleanup; not authorization. */
int mu_si_cprx_first_session_seen(const struct mu_si_cprx*);
int mu_si_cprx_init_first_video(struct mu_si_cprx*,const struct mu_owner_ops*,const struct mu_owner_session*,const MuCprxRemoteEndpoint*);
int mu_si_cprx_tick(struct mu_si_cprx*,uint64_t,void*,void*,size_t);
int mu_si_cprx_cleanup(struct mu_si_cprx*);
#endif
