#ifndef MU1005_SI_ACTIVATION_H
#define MU1005_SI_ACTIVATION_H
#include "si_mu1005_layout.h"
#define MU_SI_DP_OFF 264u
#define MU_SI_DEFERRED_APP_OFF 0x114cu
#define MU_SI_DEFERRED_ID_OFF 0x1150u
#define MU_ACT_DENIED -20
#define MU_ACT_PENDING 1
#define MU_ACT_CONNECTED 2
#define MU_ACT_UNCERTAIN -21
struct mu_act_key { uint32_t controller, id, session; uint64_t epoch; };
struct mu_act_state { struct mu_act_key key; int phase; };
struct mu_act_ops {
 void *ctx;
 struct mu_si_reader reader;
 int (*admit)(void *,const struct mu_act_key *);
 int (*observe)(void *,const struct mu_act_key *);
 void (*activate)(void *,uint32_t,uint32_t);
};
int mu_si_activate_once(struct mu_act_state*,const struct mu_act_ops*,const struct mu_act_key*);
#endif
