/* SPDX-License-Identifier: GPL-3.0-only */
#include "stock_paired.h"
#include "native_event.h"
#include <string.h>
int mu_stock_paired_snapshot(void*v,uint32_t*out){
 MuStockPaired*s=v;
 uint32_t a=0,b=0;uint8_t x[6]={0},y[6]={0};
 if(!out)return -1;
 *out=0;
 if(!s||!s->certify||!s->selected||!s->read_live||s->certify(s->ctx)!=1)return -1;
 int r=s->selected(s->ctx,&a,x);
 if(r==0)return s->certify(s->ctx)==1?0:-1;
 if(r!=1||!a)return -1;
 /* Reuse original BtLinkKeys identity-only reader; never copy keys or
  * claim that the bond itself is wireless authentication. */
 if(!mu38_stock_bond_present(x,s->read_live,s->ctx)||
    s->selected(s->ctx,&b,y)!=1||a!=b||memcmp(x,y,6)||
    s->certify(s->ctx)!=1)return -1;
 *out=a;return 1;
}
