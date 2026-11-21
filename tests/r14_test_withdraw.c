#include "inbound.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Inbound b;static int fail_add,fail_remove,fail_unreg,n,order[4],barrier_ok=1;
static int yes(void*p){(void)p;return 1;}
static uint32_t addr(const void*p){return (uint32_t)(0x10000+(const unsigned char*)p-(const unsigned char*)&b);}
static int reg(void*p,uint8_t*s,uint16_t x){assert(p==&b.rf&&!x);*s=3;return 0;}
static int unreg(void*p,uint8_t*s){assert(p==&b.rf&&*s==3);order[n++]=1;return fail_unreg;}
static int add(void*p){return p==&b.carplay?fail_add:0;}
static int rem(void*p){order[n++]=p==&b.carplay?4:2;return (p==&b.carplay)?fail_remove:0;}
static int two(void*p,int x){(void)p;(void)x;return 2;}
static int lim(void*p){(void)p;return 5035;}
static int sendp(void*p,void*q){(void)p;(void)q;return 2;}
static int dis(void*p){(void)p;return 2;}
static int opened(void*p,unsigned x){(void)p;(void)x;return 1;}
static int rx(void*p,const uint8_t*v,unsigned x){(void)p;(void)v;(void)x;return 1;}
static int credit(void*p,int x){(void)p;(void)x;return 0;}
static int barrier(void*p,const Inbound*s){(void)p;assert(s==&b);return barrier_ok;}
static void setup(void){memset(&b,0,sizeof b); n=fail_add=fail_remove=fail_unreg=0;barrier_ok=1;InboundOps o={.reg=reg,.unreg=unreg,.add=add,.remove=rem,.accept=two,.limit=lim,.send=sendp,.disconnect=dis,.certify=yes,.peer_ok=yes,.ready=yes,.opened=opened,.closed=yes,.rx=rx,.credit=credit,.addr=addr};assert(!inbound_prepare(&b,&o,123));}
int main(void){setup();assert(!inbound_publish(&b));assert(b.published_mask==7);barrier_ok=0;assert(inbound_withdraw(&b,barrier,0)==-1&&!n);barrier_ok=1;b.pending=1;assert(inbound_withdraw(&b,barrier,0)==-1&&!n);b.pending=0;assert(!inbound_withdraw(&b,barrier,0));assert(n==3&&order[0]==4&&order[1]==2&&order[2]==1&&b.state==IN_RETIRED&&!b.published_mask);assert(inbound_publish(&b)<0&&inbound_withdraw(&b,barrier,0)<0);
setup();fail_add=1;assert(inbound_publish(&b)==-2&&b.published_mask==3&&b.uncertain_mask==4);assert(inbound_withdraw(&b,barrier,0)<0&&!n);
setup();assert(!inbound_publish(&b));fail_remove=1;assert(inbound_withdraw(&b,barrier,0)==-2&&n==1&&b.published_mask==7&&b.uncertain_mask==4);assert(inbound_withdraw(&b,barrier,0)<0&&n==1);
setup();assert(!inbound_publish(&b));fail_unreg=11;assert(inbound_withdraw(&b,barrier,0)==-2&&n==3&&b.published_mask==1&&b.uncertain_mask==1);puts("PASS: reverse withdrawal, no barrier/pending rejection, failed add/remove/unreg retain uncertain objects; mock SDK only");}
