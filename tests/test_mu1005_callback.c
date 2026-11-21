/* Synthetic callback snapshots only; no real SDK owner. */
#include "cp_mu1005_callback.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int can_owner=1,can_pair=1,can_read=1;
static const uint8_t mac[6]={1,2,3,4,5,6};
static int owner(void*x){(void)x;return can_owner;}
static int pair(void*x,uint32_t assoc,uint8_t dst[6]){
 (void)x;
 if(!can_pair||assoc!=0x20000u)return 0;
 memcpy(dst,mac,6);return 1;
}
static int rd(void*x,uint32_t p,uint8_t *dst,size_t n){
 (void)x;
 if(!can_read||p!=0x30000u||n!=5)return 0;
 memcpy(dst,"hello",5);return 1;
}
static void put16(uint8_t*p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void put32(uint8_t*p,uint32_t n){for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
int main(void){
 const struct cp_mu_reader ops={0,owner,pair,rd};
 struct cp_mu_callback e;
 uint8_t packet[16]={0},rx[16]={0};
 packet[0]=1;put32(packet+12,0x20000u);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_OK);
 assert(e.code==1&&e.epoch==3&&!memcmp(e.peer_mac,mac,6));
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,2,&ops,&e,rx,sizeof rx)==CP_MU_STALE);
 can_owner=0;
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_UNVERIFIED);
 can_owner=1;can_pair=0;
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_UNVERIFIED);
 can_pair=1;packet[0]=5;put16(packet+8,5);put32(packet+12,0x30000u);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,4)==CP_MU_OVERFLOW);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_OK);
 assert(!memcmp(rx,"hello",5));
 can_read=0;memset(rx,0xA5,sizeof rx);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_UNVERIFIED);
 for(int i=0;i<5;i++)assert(rx[i]==0);
 can_read=1;put16(packet+8,5036);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_BAD);
 packet[0]=6;put16(packet+8,0);put32(packet+12,0x801200u);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_OK);
 put32(packet+12,0);
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_BAD);
 packet[0]=4;
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_OK);
 packet[0]=255;
 assert(cp_mu_decode_snapshot(packet,sizeof packet,3,3,&ops,&e,rx,sizeof rx)==CP_MU_BAD);
 /* 20k deterministic malformed or untrusted callback snapshots. */
 uint32_t state=0x12345678u;
 for(unsigned i=0;i<20000;i++){
  uint8_t f[16];for(unsigned k=0;k<16;k++){
   state=state*1664525u+1013904223u;f[k]=(uint8_t)(state>>24);
  }
  int v=cp_mu_decode_snapshot(f,sizeof f,1,1,&ops,&e,rx,sizeof rx);
  assert(v==CP_MU_OK||v==CP_MU_BAD||v==CP_MU_UNVERIFIED||v==CP_MU_OVERFLOW);
 }
 puts("AU57X_R38_CALLBACK_SNAPSHOT_20000_PASS");
 return 0;
}
