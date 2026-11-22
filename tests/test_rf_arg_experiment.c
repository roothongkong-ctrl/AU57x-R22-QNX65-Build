#include "rf_arg_experiment.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static int verified(void *p,uint8_t *v){(void)p;*v=5;return 1;}
static int unknown(void *p,uint8_t *v){(void)p;(void)v;return 0;}
int main(void){
 uint8_t v=0x55;
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_VERIFIED&&v==5);
 assert(mu_rf_arg_choose(verified,0,NULL)==MU_RF_ARG_DENY);
 v=0x55;
#if defined(AU57X_FAST_CONNECT_EXPERIMENT) && AU57X_FAST_CONNECT_EXPERIMENT == 1
 assert(mu_rf_arg_choose(unknown,0,&v)==MU_RF_ARG_EXPERIMENTAL&&v==0);
 v=0x55;
 assert(mu_rf_arg_choose(NULL,0,&v)==MU_RF_ARG_EXPERIMENTAL&&v==0);
#else
 assert(mu_rf_arg_choose(unknown,0,&v)==MU_RF_ARG_DENY&&v==0x55);
 assert(mu_rf_arg_choose(NULL,0,&v)==MU_RF_ARG_DENY&&v==0x55);
#endif
 puts("R91 RFCOMM experiment selection PASS"); return 0;
}
