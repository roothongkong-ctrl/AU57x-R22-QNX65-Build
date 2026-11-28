/* SPDX-License-Identifier: GPL-3.0-only */
#include "peer_snapshot.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct {int duplicate,stale,unbonded,broken,empty;} F;
static const uint8_t mac[6]={1,2,3,4,5,6};
static int rd(void*v,uint32_t a,void*out,size_t n){
 F*f=v;uint32_t w=0;uint32_t base=0x2d58d4u;
 if(a==0x2c1964u&&n==4){w=base;goto word;}
 if(a==0x2c3d54u&&n==4){w=1;goto word;}
 if(a==0x2c3164u&&n==2){uint8_t b[2]={1,0};memcpy(out,b,2);return 1;}
 if(a==0x2c3168u&&n==8){uint8_t b[8]={1,2,3,4,5,6,0x20,0};if(f->unbonded)b[0]=9;memcpy(out,b,8);return 1;}
 for(unsigned i=0;i<4;i++){
  uint32_t link=base+i*0xaf8u,assoc=0x700000u+i*0x100u,ch=0x710000u+i*0x100u;
  for(unsigned s=0;s<17;s++){
   uint32_t row=link+(s+1)*16u;
   int active=!f->empty&&s==5&&(i==0||(i==1&&f->duplicate));
   if(a==row+0x1bc5c8u&&n==1){*(uint8_t*)out=(uint8_t)active;return 1;}
   if(a==row+0x1bc5ccu&&n==4&&active){w=ch;goto word;}
  }
  if(a==ch+0x5du&&n==2){uint8_t b[2]={(uint8_t)i,(uint8_t)(f->broken?6:5)};memcpy(out,b,2);return 1;}
  if(a==link+0x1bc6ecu&&n==4){w=assoc;goto word;}
  if(a==assoc+13u&&n==6){memcpy(out,mac,6);if(f->stale){f->empty=1;}return 1;}
 }
 return 0;
word:
 for(unsigned k=0;k<4;k++)((uint8_t*)out)[k]=(uint8_t)(w>>(8*k));
 return 1;
}
int main(void){
 F f={0};uint32_t p=0;
 assert(mu1005_find_peer(mac,rd,&f,&p)==1&&p==0x700000u);
 uint8_t other[6]={6,5,4,3,2,1};
 p=99;assert(mu1005_find_peer(other,rd,&f,&p)==0&&p==99);
 f.duplicate=1;assert(mu1005_find_peer(mac,rd,&f,&p)==-1&&p==99);f.duplicate=0;
 f.unbonded=1;assert(mu1005_find_peer(mac,rd,&f,&p)==-1);f.unbonded=0;
 f.broken=1;assert(mu1005_find_peer(mac,rd,&f,&p)==-1);f.broken=0;
 f.empty=1;assert(mu1005_find_peer(mac,rd,&f,&p)==0);f.empty=0;
 f.stale=1;assert(mu1005_find_peer(mac,rd,&f,&p)==-1);
 puts("MU1005_SELECTED_PEER_ORIGINAL_REGISTRY_REUSE_PASS_FIXTURE_ONLY");
}
