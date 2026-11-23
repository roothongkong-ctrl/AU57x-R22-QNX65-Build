#include "rf_arg_experiment.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static int current;
static int verified(void *p,uint8_t *v){(void)p;*v=(uint8_t)current;return 1;}
static int unknown(void *p,uint8_t *v){(void)p;(void)v;return 0;}
int main(void){
 uint8_t v=0x55;
 current=5;
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_VERIFIED&&v==5);
 assert(mu_rf_arg_choose(verified,0,NULL)==MU_RF_ARG_DENY);
 current=1; assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_VERIFIED&&v==1);
 current=30;assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_VERIFIED&&v==30);
 current=0; v=0x55;
#if defined(AU57X_FAST_CONNECT_EXPERIMENT) && AU57X_FAST_CONNECT_EXPERIMENT == 1
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_EXPERIMENTAL&&v==0);
#else
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_DENY&&v==0x55);
#endif
 current=31;v=0x55;
#if defined(AU57X_FAST_CONNECT_EXPERIMENT) && AU57X_FAST_CONNECT_EXPERIMENT == 1
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_EXPERIMENTAL&&v==0);
#else
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_DENY&&v==0x55);
#endif
 current=255;v=0x55;
#if defined(AU57X_FAST_CONNECT_EXPERIMENT) && AU57X_FAST_CONNECT_EXPERIMENT == 1
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_EXPERIMENTAL&&v==0);
 assert(mu_rf_arg_choose(unknown,0,&v)==MU_RF_ARG_EXPERIMENTAL&&v==0);
#else
 assert(mu_rf_arg_choose(verified,0,&v)==MU_RF_ARG_DENY&&v==0x55);
 assert(mu_rf_arg_choose(unknown,0,&v)==MU_RF_ARG_DENY&&v==0x55);
#endif
 puts("R101 RFCOMM arg range validation: PASS (arg3 ABI still unverified)"); return 0;
}
