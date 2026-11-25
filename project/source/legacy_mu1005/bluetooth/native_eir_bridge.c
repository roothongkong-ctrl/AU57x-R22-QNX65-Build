/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original EIR builder adapter; no automatic hook or SDK call. */
#include "native_binding.h"
#include "au102_eir_advertisement.h"
int mu_bt_native_append_eir(uint8_t *buf,size_t n,size_t cap,int (*wired_idle)(void*),void*ctx,size_t*out){
 if(!buf||!out||cap<240u||n>240u)return -1;
 *out=n;
 if(n==240u||!wired_idle||wired_idle(ctx)!=1||mu_bt_native_discovery_ready()!=1)return 0;
 const Au102EirGate g={1,1,1,0};
 return au102_eir_append_if_ready(buf,n,cap,&g,out);
}

#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include "bt_notify_bridge.h"
static int allow_eir(void*p,const uint8_t*b){
 (void)p;return mu_bt_notify_discovery_allowed()&&mu_bt_native_eir_buffer_matches(b);
}
/* AAPCS replacement for BL 0x1ddfb4 at 0x12a694. The original callsite
 * still assigns r1/r2 and saves length, then submits using stock code.
 * No text mutation, no loader or automatic activation here. */
unsigned mu_bt_native_eir_builder_review(uint8_t*b,unsigned cap){
 return au102_eir_build_original_first(b,cap,
   (unsigned(*)(uint8_t*,unsigned))(uintptr_t)0x1ddfb4u,allow_eir,NULL);
}
#else
unsigned mu_bt_native_eir_builder_review(uint8_t*b,unsigned cap){(void)b;(void)cap;return 0;}
#endif
