#include "rf_arg_experiment.h"
int mu_rf_arg_choose(mu_rf_proven_cb prove, void *ctx, uint8_t *arg){
 uint8_t verified=0;
 if(!arg) return MU_RF_ARG_DENY;
 /* OEM arg3 writes into channel-state byte/halfword fields in the pinned
 * executable; a channel-number interpretation is strongly suggested but
 * NOT yet proven for iAP2. Even externally attested values must be inside
 * RFCOMM's 1..30 valid SCN space; never publish SDP for 0, 31 or 255. */
 if(prove && prove(ctx,&verified)==1 && verified>=1 && verified<=30){
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
