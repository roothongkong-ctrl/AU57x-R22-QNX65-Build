#include "bt_owner_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct{unsigned calls,fail,change,bad_code;uint8_t os[0x40],f[12],a[0xf4];} Fixture;
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static Fixture init(void){Fixture f={0};put(f.os,0x2c10d8);f.os[22]=1;put(f.os+0x3c,0x400000);put(f.f,0x2c1098);put(f.f+4,0x500000);put(f.f+8,0x600000);put(f.a,0x800000+0xc69a8);f.a[4]=1;put(f.a+0x20,9);return f;}
static int rd(void*p,uint32_t a,void*out,size_t n){Fixture*f=p;f->calls++;if(f->calls==f->fail)return 0;if(f->calls==f->change)put(f->f+4,0x500004);if(a==0x2d5644&&n==sizeof f->os)memcpy(out,f->os,n);else if(a==0x400000&&n==sizeof f->f)memcpy(out,f->f,n);else if(a==0x600000&&n==sizeof f->a)memcpy(out,f->a,n);else if(n==4){
  static const uint32_t va[]={0x4d878,0x4d87c,0x4e620,0x4e9d0,0x4ea74,0x4eab4};
  static const uint32_t word[]={0xe2800008,0xebff1d80,0xe59030f0,0xe280a0c8,0xebff1b78,0xe12fff33};
  for(unsigned i=0;i<6;i++)if(a==0x800000+va[i]){put(out,word[i]^(f->bad_code==i+1));return 1;}
  return 0;
 }else return 0;return 1;}
static void identity_cases(void){
 Fixture f=init();MuBtThreadIdentity id;
 assert(mu_bt_comm_words_match(0x800000,rd,&f)==1);
 for(unsigned i=1;i<=6;i++){f=init();f.bad_code=i;assert(!mu_bt_comm_words_match(0x800000,rd,&f));}
 f=init();assert(!mu_bt_comm_words_match(0x900000,rd,&f));
 f=init();assert(mu_bt_osapi_identity(0x800000,9,rd,&f,&id)==1);
 assert(mu_bt_osapi_identity_matches(&id,9,rd,&f)==1);
 assert(!mu_bt_osapi_identity_matches(&id,10,rd,&f));
 put(f.f+4,0x500004);
 assert(mu_bt_osapi_thread_matches(0x800000,9,rd,&f)==1);
 assert(!mu_bt_osapi_identity_matches(&id,9,rd,&f));
 f=init();
 assert(mu_bt_osapi_target(0x800000,rd,&f,&id)==1&&id.tid==9);
 assert(!mu_bt_osapi_identity(0x800000,0,rd,&f,&id)&&id.tid==0);
 assert(mu_bt_osapi_target(0x800000,rd,&f,&id)==1);
 assert(!mu_bt_osapi_identity_matches(&id,7,rd,&f));
 put(f.a+0x20,0);
 assert(!mu_bt_osapi_target(0x800000,rd,&f,&id)&&id.tid==0);
 f=init();f.change=4;assert(!mu_bt_osapi_target(0x800000,rd,&f,&id));
 puts("PASS startup discovers actual AO thread without treating caller as owner");
 puts("PASS mapped libcomm anchors and original object lifetime identity");
}
typedef struct{int bad_slot,bad_anchor,changed;unsigned got_reads;} BiasFixture;
static int bias_read(void*p,uint32_t a,void*out,size_t n){
 BiasFixture*f=p;
 const uint32_t u=0x78460000u,m=0x78310000u;
 static const uint32_t slots[]={0x2c14c4u,0x2c14d4u,0x2c13bcu,0x2c165cu};
 static const uint32_t delta[]={0x6f5f4u,0x6e3f0u,0x6fc5cu,0x4e620u};
 static const uint32_t va[]={0x4d878u,0x4d87cu,0x4e620u,0x4e9d0u,0x4ea74u,0x4eab4u};
 static const uint32_t word[]={0xe2800008u,0xebff1d80u,0xe59030f0u,0xe280a0c8u,0xebff1b78u,0xe12fff33u};
 if(n!=4)return 0;
 for(unsigned i=0;i<4;i++)if(a==slots[i]){
  uint32_t v=(i==3?m:u)+delta[i];
  ++f->got_reads;
  if(f->bad_slot==(int)i+1)v=0x117438u; /* unresolved PLT */
  if(f->changed&&f->got_reads>4&&i==3)v+=0x1000u;
  put(out,v);return 1;
 }
 for(unsigned i=0;i<6;i++)if(a==m+va[i]){put(out,word[i]);return 1;}
 if(a==u+0x6f5f4u||a==u+0x6e3f0u||a==u+0x6fc5cu){
  put(out,(a==u+0x6e3f0u?0xe5913008u:0xe92d4010u)^(f->bad_anchor!=0));return 1;
 }
 return 0;
}
static void bias_cases(void){
 BiasFixture f={0};uint32_t u=0,m=0;
 assert(mu_bt_library_biases(bias_read,&f,&u,&m)==1&&u==0x78460000u&&m==0x78310000u);
 for(int i=1;i<=4;i++){f=(BiasFixture){.bad_slot=i};u=m=0;assert(!mu_bt_library_biases(bias_read,&f,&u,&m)&&!u&&!m);}
 f=(BiasFixture){.bad_anchor=1};assert(!mu_bt_library_biases(bias_read,&f,&u,&m));
 f=(BiasFixture){.changed=1};assert(!mu_bt_library_biases(bias_read,&f,&u,&m));
 puts("PASS live GOT base discovery using recorded MU1005 mapping (mock reads; no native attach)");
}
int main(void){identity_cases();bias_cases();Fixture f=init();assert(mu_bt_osapi_thread_matches(0x800000,9,rd,&f)==1&&f.calls==6);f=init();assert(!mu_bt_osapi_thread_matches(0x800000,10,rd,&f));f=init();assert(!mu_bt_osapi_thread_matches(0x900000,9,rd,&f));f=init();f.os[22]=0;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));f=init();f.a[0xf0]=1;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));f=init();f.a[4]=0;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));f=init();f.change=4;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));for(unsigned i=1;i<=6;i++){f=init();f.fail=i;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));}f=init();assert(!mu_bt_osapi_thread_matches(0x800001,9,rd,&f)&&!f.calls);puts("PASS OSAPI ActiveObject identity, thread ID, stable lifetime (mock reads only)");}
