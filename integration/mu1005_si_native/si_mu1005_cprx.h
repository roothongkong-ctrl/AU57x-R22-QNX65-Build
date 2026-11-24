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
};
int mu_si_cprx_init(struct mu_si_cprx*,const struct mu_owner_ops*,const struct mu_owner_session*,const MuCprxRemoteEndpoint*);
int mu_si_cprx_tick(struct mu_si_cprx*,uint64_t,void*,void*,size_t);
int mu_si_cprx_cleanup(struct mu_si_cprx*);
#endif
