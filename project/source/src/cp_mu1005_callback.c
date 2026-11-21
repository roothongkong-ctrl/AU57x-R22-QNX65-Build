/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_mu1005_callback.h"
#include <string.h>
static unsigned get16(const uint8_t*p) {
 return (unsigned)p[0]|((unsigned)p[1]<<8);
}
static uint32_t get32(const uint8_t*p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int mac_ok(const uint8_t*m){
 unsigned any=0,all=255;
 for(int i=0;i<6;i++){any|=m[i];all&=m[i];}
 return any!=0&&all!=255;
}
int cp_mu_decode_snapshot(const uint8_t *s,size_t n,uint32_t expected,
             uint32_t observed,const struct cp_mu_reader *r,
             struct cp_mu_callback *out,uint8_t *rx,size_t cap){
 uint32_t token;
 unsigned code,status,len;
 if(!s||!out||!r||!r->owner_thread||!r->paired_identity||
    !r->read_live||n!=16||!expected||!observed)return CP_MU_BAD;
 memset(out,0,sizeof *out);
 if(expected!=observed)return CP_MU_STALE;
 if(r->owner_thread(r->opaque)!=1)return CP_MU_UNVERIFIED;
 code=s[0];status=get16(s+2);len=get16(s+8);token=get32(s+12);
 if(code!=1&&code!=2&&code!=4&&code!=5&&code!=6)return CP_MU_BAD;
 if(s[1]!=0)return CP_MU_BAD;
 out->code=code;out->status=status;out->length=len;
 out->token=token;out->epoch=observed;
 if(code==1||code==2){
  if(status!=0||!token||(token&3u)!=0||token>UINT32_MAX-19u ||
     r->paired_identity(r->opaque,token,out->peer_mac)!=1||
     !mac_ok(out->peer_mac))return CP_MU_UNVERIFIED;
  return CP_MU_OK;
 }
 if(code==5){
  if(status!=0||!len||len>CP_MU_MAX_PAYLOAD||!token||
     token>UINT32_MAX-(uint32_t)(len-1))return CP_MU_BAD;
  if(!rx||cap<len)return CP_MU_OVERFLOW;
  if(r->read_live(r->opaque,token,rx,len)!=1){
   memset(rx,0,len);return CP_MU_UNVERIFIED;
  }
  return CP_MU_OK;
 }
 /* TX-complete carries a descriptor token; never dereference it.
  * A real adapter must still verify the matching in-flight TX generation. */
 if(code==6 && !token)return CP_MU_BAD;
 return CP_MU_OK;
}
