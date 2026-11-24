#include "si_mu1005_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define B 0x10000u
static unsigned char m[8192];static unsigned reads;static int change,fail;
static void put(uint32_t a,uint32_t v){for(unsigned i=0;i<4;i++)m[a-B+i]=(unsigned char)(v>>(8*i));}
static int rd(void*c,uint32_t a,void*out,size_t n){(void)c;if(a<B||a-B>sizeof(m)||n>sizeof(m)-(a-B))return 0;
 if(++reads==6&&change)put(B+MU_SI_DP_OFF+MU_NATIVE_STATE_OFF,4);
 if(fail&&reads==3)return 0;
 memcpy(out,m+a-B,n);return 1;}
static void reset(void){memset(m,0,sizeof(m));reads=0;change=fail=0;put(B+264+0x488,UINT32_MAX);put(B+0x114c,UINT32_MAX);put(B+0x1150,UINT32_MAX);put(B+264+1240,0xdeadbeef);}
static int proof(void*c,const struct mu_act_key*k){(void)k;return *(int*)c;}
int main(void){struct mu_si_reader r={0,rd};struct mu_native_snapshot s;
#define READ() mu_si_snapshot(&r,B,&s)
#define CLASS() mu_si_classify(&s,17)
 reset();assert(READ()==0&&CLASS()==MU_SNAPSHOT_IDLE);
 reset();put(B+0x114c,0);put(B+0x1150,17);assert(READ()==0&&CLASS()==MU_SNAPSHOT_OWN_DEFERRED);
 reset();put(B+0x114c,0);put(B+0x1150,18);assert(READ()==0&&CLASS()==MU_SNAPSHOT_CONFLICT);
 reset();put(B+0x114c,0);assert(READ()==0&&CLASS()==MU_SNAPSHOT_CONFLICT);
 reset();put(B+264+0x488,0);put(B+264+0x498,17);put(B+264+0x48c,5);assert(READ()==0&&CLASS()==MU_SNAPSHOT_OWN_CONTEXT);
 reset();put(B+264+0x488,0);put(B+264+0x498,18);assert(READ()==0&&CLASS()==MU_SNAPSHOT_CONFLICT);
 reset();put(B+264+0x488,2);put(B+264+0x498,17);assert(READ()==0&&CLASS()==MU_SNAPSHOT_CONFLICT);
 reset();put(B+264+0x488,0);put(B+264+0x498,17);put(B+264+0x48c,19);assert(READ()==0&&CLASS()==MU_SNAPSHOT_CONFLICT);
 reset();change=1;assert(READ()==MU_SNAPSHOT_UNSTABLE);
 reset();fail=1;assert(READ()==MU_SNAPSHOT_UNSTABLE);
 reset();assert(mu_si_snapshot(&r,0xfffffff0u,&s)==MU_SNAPSHOT_UNSTABLE&&!reads);
 struct mu_act_key k={B,17,8,5};int confirmed=0;
 reset();assert(mu_si_activation_observe(&r,&k,proof,&confirmed)==0);
 put(B+264+0x488,0);put(B+264+0x498,17);put(B+264+0x48c,5);
 assert(mu_si_activation_observe(&r,&k,NULL,NULL)==1);
 assert(mu_si_activation_observe(&r,&k,proof,&confirmed)==1);
 confirmed=1;assert(mu_si_activation_observe(&r,&k,proof,&confirmed)==2);
 confirmed=-1;assert(mu_si_activation_observe(&r,&k,proof,&confirmed)==-1);
 puts("MU1005_SNAPSHOT_12_CASES_FIXTURE_ONLY_NO_OEM");return 0;}
