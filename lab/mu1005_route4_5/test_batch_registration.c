#include "si_mu1005_registration.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define B 0x10000u
static unsigned char m[16384];static unsigned mode,updated,called;static int allowed=1;
static union{uint64_t align;unsigned char v[MU_INFO_SIZE];} info;
static union{uint64_t align;unsigned char v[40];} key;
static void put(uint32_t p,uint32_t v){for(unsigned i=0;i<4;i++)m[p-B+i]=(unsigned char)(v>>(i*8));}
static int readmem(void*c,uint32_t p,void*out,size_t n){(void)c;if(p<B||p-B>sizeof m||n>sizeof m-(p-B))return 0;memcpy(out,m+p-B,n);if(mode==0&&updated)allowed=0;return 1;}
static int guard(void*c,uint32_t p,uint64_t e,uint32_t s){(void)c;return allowed&&p==B&&e==5&&s==8;}
static int32_t add(void*p,void*k,void*i){(void)p;(void)k;(void)i;called++;return 1000;}
static void update(void*p,uint32_t id,void*i){(void)p;(void)i;updated++;put(B+464,2);put(B+1028,B+3004);put(B+3004,id);if(mode==1)put(B+3000,99);}
int main(int argc,char**argv){
 assert(argc==2);mode=(unsigned)atoi(argv[1]);assert(mode<=2);
 unsigned n=mode==2?MU_RECORD_LIMIT:1;
 put(B+464,n);put(B+472,B+1024);
 for(unsigned i=0;i<n;i++){put(B+1024+4*i,B+3000+4*i);put(B+3000+4*i,i+1);}
 struct mu_reg_ops o={0,guard,add,update,{0,readmem}};uint32_t id=77;
 int rc=mu_si_register_prepared(&o,B,5,8,key.v,info.v,sizeof info.v,&id);
 assert(id==0);
 if(mode==2)assert(rc==MU_REG_DENIED&&!called&&!updated);
 else assert(rc==MU_REG_UNCERTAIN&&called==1&&updated==1);
 printf("SI_REGISTER_CASE_%u_PASS_FIXTURE_ONLY\n",mode);
}
