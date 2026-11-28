/* SPDX-License-Identifier: GPL-3.0-only */
#include "cmgr_peer.h"
#include "native_event.h"
#include <string.h>
static uint32_t le(const uint8_t*b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static int word(const MuCmgrOwner*o,uint32_t a,uint32_t*out){
 uint8_t b[4];if(o->read_live(o->ctx,a,b,4)!=1)return 0;*out=le(b);return 1;
}
/* R105.239 original exact function entry and lock/unlock. A handler
 * is NOT a 6-byte Bluetooth address. Caller proves its provenance. */
int mu1005_cmgr_contract(const MuCmgrOwner*o,uint32_t handler){
 static const uint32_t anchor[][2]={
 {0x242670u,0xe92d4010u},{0x242674u,0xe1a04000u},
 {0x242678u,0xeb00deabu},{0x24267cu,0xe1a00004u},
 {0x242680u,0xebfffff9u},{0x242684u,0xe3500000u},
 {0x242688u,0x01a04000u},{0x24268cu,0x1594300cu},
 {0x242690u,0x1593402cu},{0x242694u,0xeb00de81u},
 {0x242698u,0xe1a00004u},{0x24269cu,0xe8bd8010u}
 };
 uint32_t v;
 if(!o||!o->certify||!o->read_live||!handler||(handler&3u)||handler>UINT32_MAX-15u||
    o->certify(o->ctx,handler)!=1)return 0;
 for(size_t i=0;i<sizeof anchor/sizeof anchor[0];i++)
  if(!word(o,anchor[i][0],&v)||v!=anchor[i][1])return 0;
 return o->certify(o->ctx,handler)==1;
}
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
int mu1005_cmgr_remote(const MuCmgrOwner*o,uint32_t handler,const uint8_t selected[6],uint32_t*out){
 if(!out||!selected||!mu1005_cmgr_contract(o,handler))return -1;
 unsigned any=0,all=255;
 for(unsigned i=0;i<6;i++){any|=selected[i];all&=selected[i];}
 if(!any||all==255)return -1;
 if(!mu38_stock_bond_present(selected,o->read_live,o->ctx))return -1;
 typedef void *(*NativeGet)(void*);
 NativeGet get=(NativeGet)(uintptr_t)0x242670u;
 if(o->certify(o->ctx,handler)!=1)return -1;
 /* Call actual original locking getter, only on certified original owner. */
 uint32_t device=(uint32_t)(uintptr_t)get((void*)(uintptr_t)handler);
 if(o->certify(o->ctx,handler)!=1)return -1;
 if(!device)return 0;
 uint8_t mac[6];uint32_t relation,observed;
 if((device&3u)||device>UINT32_MAX-18u||
    !word(o,handler+0xcu,&relation)||!relation||(relation&3u)||relation>UINT32_MAX-0x2fu||
    !word(o,relation+0x2cu,&observed)||observed!=device||
    o->read_live(o->ctx,device+0xdu,mac,6)!=1||memcmp(mac,selected,6)||
    o->certify(o->ctx,handler)!=1)return -1;
 *out=device;return 1;
}
#else
int mu1005_cmgr_remote(const MuCmgrOwner*o,uint32_t handler,const uint8_t selected[6],uint32_t*out){
 (void)o;(void)handler;(void)selected;(void)out;return -1;
}
#endif
