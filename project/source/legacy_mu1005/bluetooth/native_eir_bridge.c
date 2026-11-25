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
