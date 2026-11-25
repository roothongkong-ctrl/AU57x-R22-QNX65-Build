#include "rf_arg_experiment.h"
int mu_rf_arg_choose(mu_rf_proven_cb prove, void *ctx, uint8_t *arg){
 uint8_t verified=0;
 if(!arg) return MU_RF_ARG_DENY;
 /* Initial RX credit budget; native PN response clamps to 7. */
 if(prove && prove(ctx,&verified)==1 && verified>=1 && verified<=7){
  *arg=verified; return MU_RF_ARG_VERIFIED;
 }
#if defined(AU57X_FAST_CONNECT_EXPERIMENT) && AU57X_FAST_CONNECT_EXPERIMENT == 1
 /* Explicit risky test setting: donor passes 0, but the exact AU57x
  * interpretation is not proven. SDK may reject it or break BT service. */
 *arg=0;
 return MU_RF_ARG_EXPERIMENTAL;
#else
 (void)ctx;
 return MU_RF_ARG_DENY;
#endif
}
