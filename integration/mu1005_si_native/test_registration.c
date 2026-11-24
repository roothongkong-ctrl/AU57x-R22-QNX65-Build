#include "si_mu1005_registration.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define B 0x10000u
static unsigned char m[4096];static unsigned calls, updates, allow, revoke, no_insert;static int32_t result;
static union{uint64_t align;unsigned char bytes[MU_INFO_SIZE];} info;
static union{uint64_t align;unsigned char bytes[40];} key;
static void put(uint32_t p,uint32_t v){for(unsigned i=0;i<4;i++)m[p-B+i]=(unsigned char)(v>>(i*8));}
static int rd(void*c,uint32_t p,void*out,size_t n){(void)c;if(p<B||p-B>sizeof(m)||n>sizeof(m)-(p-B))return 0;memcpy(out,m+p-B,n);return 1;}
static int guard(void*c,uint32_t p,uint64_t e,uint32_t s){(void)c;return allow&&p==B&&e==5&&s==8;}
static int32_t reg(void*p,void*k,void*secondary){assert((uintptr_t)p==B&&k==key.bytes&&secondary==info.bytes+24);calls++;((unsigned char*)secondary)[0]=0x42;if(revoke)allow=0;return result;}
static void update(void*p,uint32_t id,void*i){assert((uintptr_t)p==B&&i==info.bytes&&id==(uint32_t)result&&info.bytes[24]==0x42);updates++;if(!no_insert){put(B+464,2);put(B+1028,B+1800);put(B+1800,id);}}
static void reset(void){memset(m,0,sizeof(m));memset(info.bytes,0,sizeof(info.bytes));calls=updates=revoke=no_insert=0;allow=1;result=18;put(B+464,1);put(B+472,B+1024);put(B+1024,B+1536);put(B+1536,17);}
int main(void){struct mu_reg_ops o={0,guard,reg,update,{0,rd}};uint32_t id;int rc;
#define CALL() mu_si_register_prepared(&o,B,5,8,key.bytes,info.bytes,sizeof(info.bytes),&id)
 reset();assert(CALL()==0&&id==18&&calls==1&&updates==1);puts("PASS three native arguments; mutated secondary passed into update");
 reset();allow=0;assert(CALL()==MU_REG_DENIED&&id==0&&!calls&&!updates);puts("PASS owner rejected");
 reset();assert(mu_si_register_prepared(&o,B,5,8,info.bytes+64,info.bytes,sizeof(info.bytes),&id)==MU_REG_DENIED&&!calls);puts("PASS aliased native key rejected");
 reset();assert(mu_si_register_prepared(&o,B,5,8,key.bytes,info.bytes,200,&id)==MU_REG_DENIED&&!calls);puts("PASS old donor size rejected");
 reset();result=17;assert(CALL()==MU_REG_UNCERTAIN&&id==0&&calls==1&&!updates);puts("PASS original ID preserved");
 reset();result=-1;assert(CALL()==MU_REG_UNCERTAIN&&id==0&&!updates);puts("PASS native failure no retry");
 reset();revoke=1;assert(CALL()==MU_REG_UNCERTAIN&&id==0&&!updates);puts("PASS owner revoked during native registration");
 reset();no_insert=1;assert(CALL()==MU_REG_UNCERTAIN&&id==0&&updates==1);puts("PASS absent result not reported as registered");
 reset();put(B+464,101);rc=CALL();assert(rc==MU_REG_DENIED&&!calls&&!updates);puts("PASS invalid native list no write");
 puts("MU1005_REGISTRATION_ABI_9_CASES_OK_FIXTURE_ONLY_NO_OEM");return 0;}
