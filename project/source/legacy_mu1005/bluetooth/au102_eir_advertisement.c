/* Original donor implementation in third_party/luka-dev/.../eir.c (GPLv3).
 * This adaptor is written for AU57x and must not be treated as a verified
 * OEM BlueSDK EIR callback hook. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "au102_eir_advertisement.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
extern unsigned int eir_append_carplay_uuid(unsigned char *, unsigned int);
static const uint8_t uuid[16]={0xd3,0x1f,0xbf,0x50,0x5d,0x57,0x27,0x97,0xa2,0x40,0x41,0xcd,0x48,0x43,0x88,0xec};
/* Scan ALL fields before donor mutates the first complete UUID list.
 * If stock contains an incomplete 128-bit list, do not create a
 * contradictory complete list. Preserve the original bytes. */
static int eligible(const uint8_t*b,size_t n){
 for(size_t i=0;i<n;){
  size_t len=b[i];
  if(!len||len>n-i-1)return 0;
  if(b[i+1]==6||b[i+1]==7){
   if((len-1)%16)return 0;
   for(size_t j=i+2;j<i+1+len;j+=16)
    if(!memcmp(b+j,uuid,16))return 0;
   if(b[i+1]==6)return 0;
  }
  i+=len+1;
 }
 return 1;
}
int au102_eir_append_if_ready(uint8_t *eir, size_t length, size_t capacity,
                               const Au102EirGate *gate, size_t *out_length) {
    unsigned int new_len;
    if (!out_length || !gate || !eir || capacity < 240u || length >= 240u)
        return -1;
    *out_length = length;
    if (gate->wired_carplay_active)
        return 0;
    if (gate->stock_btstack_verified != 1 ||
        gate->iap2_rfcomm_registered != 1 ||
        gate->iap2_sdp_published != 1)
        return 0;
    if (!eligible(eir,length)) return 0;
    uint8_t scratch[240];
    memcpy(scratch,eir,sizeof scratch);
    new_len = eir_append_carplay_uuid(scratch, (unsigned int)length);
    if (new_len < length || new_len >= 240u)
        return -1;
    if(new_len!=length)memcpy(eir,scratch,sizeof scratch);
    *out_length = new_len;
    return new_len == length ? 0 : 1;
}

/* The stock builder's return value is authoritative on any declined
 * augmentation. Exact call-site capacity, not an inferred buffer size. */
unsigned au102_eir_build_original_first(uint8_t*b,unsigned cap,
 unsigned(*original)(uint8_t*,unsigned),int(*allow)(void*,const uint8_t*),void*ctx){
 if(!original)return 0;
 unsigned n=original(b,cap);
 if(!b||cap!=240u||n==0||n>=cap||!allow||allow(ctx,b)!=1)return n;
 uint8_t scratch[240];size_t out=n;const Au102EirGate g={1,1,1,0};
 memcpy(scratch,b,sizeof scratch);
 if(au102_eir_append_if_ready(scratch,n,cap,&g,&out)!=1||allow(ctx,b)!=1)return n;
 memcpy(b,scratch,sizeof scratch);return (unsigned)out;
}
