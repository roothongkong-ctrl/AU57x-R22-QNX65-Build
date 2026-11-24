#ifndef MU1005_SI_SNAPSHOT_H
#define MU1005_SI_SNAPSHOT_H
#include "si_mu1005_activation.h"
/* Raw state is not an AirPlay/MFi success proof. Owner thread only. */
#define MU_NATIVE_APP_OFF 0x488u
#define MU_NATIVE_STATE_OFF 0x48cu
#define MU_NATIVE_ID_OFF 0x498u
#define MU_SNAPSHOT_UNSTABLE -30
#define MU_SNAPSHOT_CONFLICT -31
#define MU_SNAPSHOT_IDLE 0
#define MU_SNAPSHOT_OWN_DEFERRED 1
#define MU_SNAPSHOT_OWN_CONTEXT 2
struct mu_native_snapshot { uint32_t app,state,id,deferred_app,deferred_id; };
int mu_si_snapshot(const struct mu_si_reader*,uint32_t controller,struct mu_native_snapshot*);
int mu_si_classify(const struct mu_native_snapshot*,uint32_t expected_id);
int mu_si_activation_observe(const struct mu_si_reader*,const struct mu_act_key*,
    int (*receiver_confirmed)(void*,const struct mu_act_key*),void*);
#endif
