#include "si_mu1005_activation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define B 0x10000u
static unsigned char mem[65536];static int live=1,observations,activations,mode;
static void put(uint32_t p,uint32_t n){assert(p>=B&&p-B+4<=sizeof mem);for(unsigned i=0;i<4;i++)mem[p-B+i]=(unsigned char)(n>>(8*i));}
static int readmem(void*c,uint32_t p,void*out,size_t n){(void)c;if(p<B||p-B>sizeof mem||n>sizeof mem-(p-B))return 0;memcpy(out,mem+p-B,n);return 1;}
static int admit(void*c,const struct mu_act_key*k){(void)c;(void)k;return live;}
static int observe(void*c,const struct mu_act_key*k){(void)c;(void)k;observations++;if(observations==1)return 0;if(mode==0||observations==3)live=0;return 2;}
static void activate(void*c,uint32_t id,unsigned arg){(void)c;assert(id==7&&arg==0);activations++;}
int main(void){
 uint32_t p=B+MU_SI_DP_OFF;put(p+464,1);put(p+472,B+0x4000);put(B+0x4000,B+0x5000);put(B+0x5000,7);
 struct mu_act_key k={B,7,8,9};struct mu_act_ops o={.reader={0,readmem},.admit=admit,.observe=observe,.activate=activate};
 for(mode=0;mode<2;mode++){struct mu_act_state s={0};live=1;observations=activations=0;int rc=mu_si_activate_once(&s,&o,&k);
 if(mode){assert(rc==MU_ACT_CONNECTED);rc=mu_si_activate_once(&s,&o,&k);}
 assert(rc==MU_ACT_UNCERTAIN&&activations==1);live=1;assert(mu_si_activate_once(&s,&o,&k)==MU_ACT_UNCERTAIN);}
 puts("SI_ACTIVATION_REVOKED_OBSERVATION_PASS_FIXTURE_ONLY");
}
