/* SPDX-License-Identifier: GPL-3.0-only */
#include "rf_client.h"
#include "cp_wcp_autoconnect.h"
static int rd(const MuRfClientOwner*o,uint32_t a,void*b,size_t n){
 return o->cmgr.read_live(o->cmgr.ctx,a,b,n)==1;
}
static int word(const MuRfClientOwner*o,uint32_t a,uint32_t*v){
 uint8_t b[4];if(!rd(o,a,b,4))return 0;
 *v=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;return 1;
}
int mu1005_rf_client_contract(const MuRfClientOwner*o,uint32_t handler,uint32_t remote,uint32_t descriptor,unsigned scn,unsigned profile){
 /* Entry checks reproduced from R105.239 exact ELF disassembly, not
  * inferred donor offsets. The original callback must be present. */
 static const uint32_t anchors[][2]={
 {0x2774c4u,0xe92d47f0u},{0x2774c8u,0xe2508000u},
 {0x2774ccu,0xe59f44ccu},{0x2774d0u,0xe24dd020u},
 {0x2774d4u,0xe1a07001u},{0x2774d8u,0xe1a05002u},
 {0x2774e4u,0xe2412001u},{0x2774e8u,0xe6ef2072u},
 {0x2774ecu,0xe352001du},{0x2774fcu,0xe5952008u},
 {0x277508u,0xe5d5200eu},{0x27750cu,0xe352003fu},
 {0x277514u,0xe1d520bcu},{0x277518u,0xe30113abu},
 {0x27751cu,0xe1520001u}
 };
 uint32_t v;uint8_t fields[3];
 if(!o||!o->client_ready||!remote||(remote&3u)||!descriptor||(descriptor&3u)||
    descriptor>UINT32_MAX-15u||scn<1||scn>30||profile<1||profile>7||
    !mu1005_cmgr_contract(&o->cmgr,handler))return 0;
 for(size_t i=0;i<sizeof anchors/sizeof anchors[0];i++)
  if(!word(o,anchors[i][0],&v)||v!=anchors[i][1])return 0;
 if(!word(o,descriptor+8u,&v)||!v||!rd(o,descriptor+12u,fields,3)||
    fields[2]>63u||((unsigned)fields[0]|((unsigned)fields[1]<<8))>0x13abu)return 0;
 return o->client_ready(o->cmgr.ctx,handler,remote,descriptor,scn,profile)==1&&
        o->cmgr.certify(o->cmgr.ctx,handler)==1;
}
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
int mu1005_rf_client_open(const MuRfClientOwner*o,uint32_t handler,const uint8_t selected[6],uint32_t descriptor,unsigned scn,unsigned profile){
 uint32_t remote=0;
 if(!o||mu1005_cmgr_remote(&o->cmgr,handler,selected,&remote)!=1||
    !mu1005_rf_client_contract(o,handler,remote,descriptor,scn,profile))return CP_WCP_OPEN_REJECTED;
 typedef int(*NativeOpen)(void*,uint8_t,void*,uint16_t);
 NativeOpen open=(NativeOpen)(uintptr_t)0x2774c4u;
 int raw=open((void*)(uintptr_t)remote,(uint8_t)scn,(void*)(uintptr_t)descriptor,(uint16_t)profile);
 /* Even if ownership changes in the original call, a request may exist.
  * Non-2 results also require actual cancellation/release evidence. */
 if(o->cmgr.certify(o->cmgr.ctx,handler)!=1)return CP_WCP_OPEN_UNCERTAIN;
 return cp_wcp_stock_client_result(raw,0);
}
#else
int mu1005_rf_client_open(const MuRfClientOwner*o,uint32_t h,const uint8_t m[6],uint32_t d,unsigned s,unsigned p){
 (void)o;(void)h;(void)m;(void)d;(void)s;(void)p;return CP_WCP_OPEN_REJECTED;
}
#endif
