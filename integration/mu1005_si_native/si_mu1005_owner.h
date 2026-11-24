#ifndef MU1005_SI_OWNER_H
#define MU1005_SI_OWNER_H
#include "si_mu1005_registration.h"
#include "si_mu1005_snapshot.h"
#define MU_OWNER_DENIED -40
#define MU_OWNER_UNCERTAIN -41
#define MU_OWNER_PENDING 1
#define MU_OWNER_CONNECTED 2
struct mu_owner_session { uint32_t controller,session; uint64_t epoch; };
struct mu_owner_state { struct mu_owner_session owner; struct mu_act_state activation; uint32_t id; int phase; };
/* Serialized original owner thread only. Cleanup confirmation must be independent. */
struct mu_owner_ops {
 struct mu_reg_ops registration;
 void *context;
 int (*admitted)(void*,const struct mu_owner_session*);
 int (*receiver_confirmed)(void*,const struct mu_act_key*);
 void (*activate)(void*,uint32_t,uint32_t);
 int (*cleanup_confirmed)(void*,const struct mu_owner_session*,uint32_t);
};
int mu_owner_tick(struct mu_owner_state*,const struct mu_owner_ops*,const struct mu_owner_session*,void*,void*,size_t);
int mu_owner_clear_after_cleanup(struct mu_owner_state*,const struct mu_owner_ops*,const struct mu_owner_session*);
#endif
