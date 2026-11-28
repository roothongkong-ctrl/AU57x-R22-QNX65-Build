/* SPDX-License-Identifier: GPL-3.0-only */
#include "cmgr_peer.h"
#include <assert.h>
#include <stdio.h>
typedef struct{int bad,allow;unsigned reads;} F;
static int cert(void*v,uint32_t h){return ((F*)v)->allow&&h==0x700000u;}
static int readword(void*v,uint32_t a,void*b,size_t n){
 F*f=v;const uint32_t words[]={0xe92d4010u,0xe1a04000u,0xeb00deabu,0xe1a00004u,0xebfffff9u,0xe3500000u,0x01a04000u,0x1594300cu,0x1593402cu,0xeb00de81u,0xe1a00004u,0xe8bd8010u};
 if(n!=4||a<0x242670u||a>0x24269cu||(a&3u))return 0;
 unsigned i=(a-0x242670u)/4u;uint32_t w=words[i]^(f->bad==(int)i+1);
 f->reads++;for(unsigned k=0;k<4;k++)((uint8_t*)b)[k]=(uint8_t)(w>>(8*k));return 1;
}
int main(void){
 F f={0,1,0};MuCmgrOwner o={&f,cert,readword};
 assert(mu1005_cmgr_contract(&o,0x700000u)&&f.reads==12);
 for(int i=1;i<=12;i++){f.bad=i;assert(!mu1005_cmgr_contract(&o,0x700000u));}
 f.bad=0;f.allow=0;assert(!mu1005_cmgr_contract(&o,0x700000u));
 f.allow=1;assert(!mu1005_cmgr_contract(&o,0x700001u));
 uint8_t mac[6]={1,2,3,4,5,6};uint32_t out=99;
 assert(mu1005_cmgr_remote(&o,0x700000u,mac,&out)==-1&&out==99);
 puts("CMGR_HANDLER_LOCK_CONTRACT_PASS_HOST_NO_NATIVE_CALL");
}
