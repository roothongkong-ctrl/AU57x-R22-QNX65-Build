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
 else if(a==0x2c3d54u&&n==4)put(p,1);
 else if(a==0x2c3d48u&&n==4)put(p,0x10001cu);
 else if(a==0x10001cu&&n==4)put(p,0x2be61cu);
 else return 0;
 return 1;
}
static F fixture(void){F f={0};put(f.sv,0x2be500);f.started=1;put(f.os,0x2c10d8);for(unsigned i=1;i<=4;i++)put(f.os+i*4,0x400000+i*8);f.os[22]=1;return f;}
static int lifecycle_read(void*ctx,uint32_t a,void*p,size_t n){
 static const uint32_t addr[]={0x2be500,0x11e9b0,0x11eab0,0x11ec94,0x11ec98,0x125908,0x125968,0x125970,0x11de34,0x11de54};
 static const uint32_t val[]={0x11e9b0,0xe92d4ff0,0xeb0571b6,0xe3a03001,0xe5c531b0,0xe92d4070,0xeb055604,0xe5c531b0,0xeb0574d1,0xe5c831b0};
 if(n!=4)return 0;
 for(unsigned i=0;i<10;i++)if(a==addr[i]){put(p,val[i]^(*(unsigned*)ctx==i+1));return 1;}
 return 0;
}
int main(void){
 unsigned bad=0;assert(mu_bt_sdk_lifecycle_sites(lifecycle_read,&bad)==1);
 for(bad=1;bad<=10;bad++)assert(!mu_bt_sdk_lifecycle_sites(lifecycle_read,&bad));
 puts("PASS exact OEM startup and shutdown instruction sites (fixture; lease still required)");
 F f=fixture();assert(mu_bt_sdk_service_address(readmem,&f)==0x100000u);
 f=fixture();assert(mu_bt_sdk_started(0x100000,readmem,&f)==1&&f.calls==6);
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
