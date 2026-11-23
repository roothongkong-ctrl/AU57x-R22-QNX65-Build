#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../project/source/legacy_mu1005/bluetooth/au102_eir_advertisement.h"
static const uint8_t uuid[16] = {0xd3,0x1f,0xbf,0x50,0x5d,0x57,0x27,0x97,0xa2,0x40,0x41,0xcd,0x48,0x43,0x88,0xec};
static Au102EirGate yes = {1,1,1,0};
static void negative_gates(void){
  uint8_t b[240]={2,1,6}, before[240];size_t n=0;
  Au102EirGate g=yes;
  memcpy(before,b,240);g.stock_btstack_verified=0;
  assert(au102_eir_append_if_ready(b,3,240,&g,&n)==0&&n==3&&!memcmp(b,before,240));
  g=yes;g.iap2_rfcomm_registered=0;
  assert(au102_eir_append_if_ready(b,3,240,&g,&n)==0&&!memcmp(b,before,240));
  g=yes;g.iap2_sdp_published=0;
  assert(au102_eir_append_if_ready(b,3,240,&g,&n)==0&&!memcmp(b,before,240));
  g=yes;g.wired_carplay_active=1;
  assert(au102_eir_append_if_ready(b,3,240,&g,&n)==0&&!memcmp(b,before,240));
  assert(au102_eir_append_if_ready(b,3,239,&yes,&n)==-1&&!memcmp(b,before,240));
  assert(au102_eir_append_if_ready(b,3,240,&yes,NULL)==-1);
}
static void add_and_duplicate(void){
 uint8_t b[240]={2,1,6};size_t n=0;
 assert(au102_eir_append_if_ready(b,3,240,&yes,&n)==1 && n==21);
 assert(b[3]==17 && b[4]==7 && memcmp(b+5,uuid,16)==0);
 assert(au102_eir_append_if_ready(b,n,240,&yes,&n)==0 && n==21);
}
static void extend_existing_uuid_field(void){
 uint8_t b[240]={17,7};size_t n=0;
 for(int i=0;i<16;i++)b[2+i]=(uint8_t)i;
 assert(au102_eir_append_if_ready(b,18,240,&yes,&n)==1 && n==34);
 assert(b[0]==33&&memcmp(b+18,uuid,16)==0);
 assert(au102_eir_append_if_ready(b,n,240,&yes,&n)==0 && n==34);
}
static void malformed_is_unchanged(void){
 uint8_t b[240]={4,1,0}, before[240];size_t n=0;
 memcpy(before,b,240);
 assert(au102_eir_append_if_ready(b,3,240,&yes,&n)==0&&n==3&&!memcmp(b,before,240));
}
static void full_is_unchanged(void){
 uint8_t b[240]={0},before[240];size_t n=0;
 /* A non-UUID AD field that leaves too little room. */
 b[0]=234;b[1]=0x09; for(int i=2;i<235;i++)b[i]=65;
 memcpy(before,b,240);
 assert(au102_eir_append_if_ready(b,235,240,&yes,&n)==0 && n==235 && !memcmp(before,b,240));
}
int main(void){negative_gates();add_and_duplicate();extend_existing_uuid_field();malformed_is_unchanged();full_is_unchanged();puts("R102 imported GPL upstream EIR + AU57x gate: PASS");return 0;}
