/* SPDX-License-Identifier: GPL-3.0-only */
#include "peer_snapshot.h"
#include "native_event.h"
#include <string.h>
typedef int (*Read)(void*,uint32_t,void*,size_t);
static int word(Read r,void*c,uint32_t a,uint32_t*out){
 uint8_t b[4];if(r(c,a,b,4)!=1)return 0;
 *out=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
 return 1;
}
static int scan(const uint8_t selected[6],Read r,void*c,uint32_t*out){
 uint32_t base,found=0;
 if(!word(r,c,0x2c1964u,&base)||base!=0x2d58d4u)return -1;
 for(unsigned i=0;i<4;i++){
  uint32_t link=base+i*0xaf8u;
  for(unsigned s=0;s<17;s++){
   uint32_t row=link+(s+1u)*16u,ch,assoc;
   uint8_t flags,ix[2],mac[6];
   if(r(c,row+0x1bc5c8u,&flags,1)!=1)return -1;
   if(!(flags&1u))continue;
   if(!word(r,c,row+0x1bc5ccu,&ch)||!ch||(ch&3u)||ch>UINT32_MAX-0x5eu||
      r(c,ch+0x5du,ix,2)!=1||ix[0]!=i||ix[1]!=s||
      !word(r,c,link+0x1bc6ecu,&assoc)||!assoc||(assoc&3u)||assoc>UINT32_MAX-18u||
      !mu38_registry_association(ch,assoc,r,c)||
      r(c,assoc+13u,mac,6)!=1)return -1;
   if(!memcmp(mac,selected,6)){
    if(found&&found!=assoc)return -1;
    found=assoc;
   }
  }
 }
 *out=found;return found?1:0;
}
int mu1005_find_peer(const uint8_t selected[6],Read r,void*c,uint32_t*out){
 if(!selected||!r||!out)return -1;
 unsigned any=0,all=255;
 for(unsigned i=0;i<6;i++){any|=selected[i];all&=selected[i];}
 if(!any||all==255)return -1;
 uint32_t a=0,b=0;
 int first=scan(selected,r,c,&a);
 if(first<0)return -1;
 if(first==1&&!mu38_stock_bond_present(selected,r,c))return -1;
 int second=scan(selected,r,c,&b);
 if(second!=first||a!=b)return -1;
 if(first==1)*out=a;
 return first;
}
