/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_startup_snapshot.h"
#include <string.h>
typedef struct {uint8_t service_vptr[4], service_started, os[24];} Snapshot;
static uint32_t le(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int ptr(uint32_t x){return x&&!(x&3u);}
static int capture(uint32_t s,int(*r)(void*,uint32_t,void*,size_t),void*c,Snapshot*out){
 memset(out,0,sizeof *out);
 return r(c,s,out->service_vptr,4)==1&&r(c,s+0x1b0u,&out->service_started,1)==1&&r(c,0x2d5644u,out->os,sizeof out->os)==1;
}
int mu_bt_sdk_started(uint32_t s,int(*r)(void*,uint32_t,void*,size_t),void*c){
 Snapshot a,b;
 if(!r||!ptr(s)||s>UINT32_MAX-0x1b0u||!capture(s,r,c,&a)||!capture(s,r,c,&b)||memcmp(&a,&b,sizeof a))return 0;
 if(le(a.service_vptr)!=0x2be500u||a.service_started!=1u||le(a.os)!=0x2c10d8u||a.os[0x16]!=1u)return 0;
 /* Distinct initialized mutexes, plus SDK timer object; not a lock,
  * callback, MFi, or thread ownership proof. */
 uint32_t x=le(a.os+4),y=le(a.os+8),z=le(a.os+12),t=le(a.os+16);
 return ptr(x)&&ptr(y)&&ptr(z)&&ptr(t)&&x!=y&&y!=z&&x!=z&&t!=x&&t!=y&&t!=z;
}

uint32_t mu_bt_sdk_service_address(int(*r)(void*,uint32_t,void*,size_t),void*c){
 uint8_t b[4];uint32_t listener,service;
 /* BtLinkKeys singleton +0xbe4 is BluetoothServices this+0x1c. */
 if(!r||r(c,0x2c3d54u,b,4)!=1||!(le(b)&1u)||
    r(c,0x2c3d48u,b,4)!=1)return 0;
 listener=le(b);
 if(!ptr(listener)||listener<=0x1cu)return 0;
 service=listener-0x1cu;
 if(r(c,listener,b,4)!=1||le(b)!=0x2be61cu||
    !mu_bt_sdk_started(service,r,c)||
    r(c,0x2c3d48u,b,4)!=1||le(b)!=listener)return 0;
 return service;
}
