/* R91: isolated fast-connect RFCOMM argument choice, no car verification. */
#ifndef MU1005_RF_ARG_EXPERIMENT_H
#define MU1005_RF_ARG_EXPERIMENT_H
#include <stdint.h>
typedef int (*mu_rf_proven_cb)(void *ctx, uint8_t *arg);
enum { MU_RF_ARG_VERIFIED=1, MU_RF_ARG_EXPERIMENTAL=2, MU_RF_ARG_DENY=-1 };
/* Owner-attested initial RX credits 1..7, not SCN; zero is experimental only. */
int mu_rf_arg_choose(mu_rf_proven_cb prove, void *ctx, uint8_t *arg);
#endif
