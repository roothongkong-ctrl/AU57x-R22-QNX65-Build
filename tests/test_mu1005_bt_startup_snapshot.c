#include "bt_startup_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint8_t sv[4],started,os[24];unsigned calls,change_at,fail_at;} F;
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static int readmem(void*ctx,uint32_t a,void*p,size_t n){
 F*f=ctx;++f->calls;if(f->calls==f->fail_at)return 0;
 if(f->calls==f->change_at)f->os[22]=0;
 if(a==0x100000u&&n==4)memcpy(p,f->sv,n);
 else if(a==0x1001b0u&&n==1)memcpy(p,&f->started,n);
 else if(a==0x2d5644u&&n==24)memcpy(p,f->os,n);
 else return 0;
 return 1;
}
static F fixture(void){F f={0};put(f.sv,0x2be500);f.started=1;put(f.os,0x2c10d8);for(unsigned i=1;i<=4;i++)put(f.os+i*4,0x400000+i*8);f.os[22]=1;return f;}
int main(void){
 F f=fixture();assert(mu_bt_sdk_started(0x100000,readmem,&f)==1&&f.calls==6);
 f=fixture();f.os[22]=0;assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 f=fixture();f.started=0;assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 f=fixture();f.sv[0]^=1;assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 f=fixture();f.os[0]^=1;assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 f=fixture();memset(f.os+16,0,4);assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 f=fixture();memcpy(f.os+8,f.os+4,4);assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 f=fixture();f.change_at=4;assert(!mu_bt_sdk_started(0x100000,readmem,&f));
 for(unsigned i=1;i<=6;i++){f=fixture();f.fail_at=i;assert(!mu_bt_sdk_started(0x100000,readmem,&f));}
 f=fixture();assert(!mu_bt_sdk_started(0xfffffffc,readmem,&f)&&!f.calls);
 assert(!mu_bt_sdk_started(0x100001,readmem,&f)&&!f.calls);
 puts("PASS OEM startup snapshot: actual SDK state distinct from outer start flag; mock reads only");
}
