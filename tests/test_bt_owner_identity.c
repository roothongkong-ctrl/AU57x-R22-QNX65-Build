#include "bt_owner_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct{unsigned calls,fail,change;uint8_t os[0x40],f[12],a[0xf4];} Fixture;
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static Fixture init(void){Fixture f={0};put(f.os,0x2c10d8);f.os[22]=1;put(f.os+0x3c,0x400000);put(f.f,0x2c1098);put(f.f+4,0x500000);put(f.f+8,0x600000);put(f.a,0x800000+0xc69a8);f.a[4]=1;put(f.a+0x20,9);return f;}
static int rd(void*p,uint32_t a,void*out,size_t n){Fixture*f=p;f->calls++;if(f->calls==f->fail)return 0;if(f->calls==f->change)put(f->f+4,0x500004);if(a==0x2d5644&&n==sizeof f->os)memcpy(out,f->os,n);else if(a==0x400000&&n==sizeof f->f)memcpy(out,f->f,n);else if(a==0x600000&&n==sizeof f->a)memcpy(out,f->a,n);else return 0;return 1;}
int main(void){Fixture f=init();assert(mu_bt_osapi_thread_matches(0x800000,9,rd,&f)==1&&f.calls==6);f=init();assert(!mu_bt_osapi_thread_matches(0x800000,10,rd,&f));f=init();assert(!mu_bt_osapi_thread_matches(0x900000,9,rd,&f));f=init();f.os[22]=0;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));f=init();f.a[0xf0]=1;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));f=init();f.a[4]=0;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));f=init();f.change=4;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));for(unsigned i=1;i<=6;i++){f=init();f.fail=i;assert(!mu_bt_osapi_thread_matches(0x800000,9,rd,&f));}f=init();assert(!mu_bt_osapi_thread_matches(0x800001,9,rd,&f)&&!f.calls);puts("PASS OSAPI ActiveObject identity, thread ID, stable lifetime (mock reads only)");}
