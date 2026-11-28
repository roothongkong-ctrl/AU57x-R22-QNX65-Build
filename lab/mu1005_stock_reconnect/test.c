/* SPDX-License-Identifier: GPL-3.0-only */
#include "stock_reconnect.h"
#include <assert.h>
#include <stdio.h>
typedef struct{uint32_t gen;int acl,idle,opens,cancels;} F;
static int snapshot(void*v,uint32_t*g){F*f=v;*g=f->gen;return f->acl;}
static unsigned take(void*v){(void)v;return 0;}
static int owner(void*v,uint32_t g){return ((F*)v)->gen==g;}
static int pair(void*v,uint32_t g){return owner(v,g)&&((F*)v)->acl;}
static int idle(void*v){return ((F*)v)->idle;}
static int no_link(void*v,uint32_t g){(void)v;(void)g;return 0;}
static int open_iap2(void*v,uint32_t g){F*f=v;assert(pair(v,g));f->opens++;return CP_WCP_OPEN_PENDING;}
static int cancel(void*v,uint32_t g){F*f=v;(void)g;f->cancels++;return 1;}
static int release(void*v,uint32_t g){(void)v;(void)g;return 1;}
int main(void){
 F f={7,0,0,0,0};MuPairedStart s;
 struct cp_wcp_bt_ops o={&f,owner,pair,idle,no_link,pair,open_iap2,cancel};
 assert(mu_paired_start_init(&s,&o,snapshot,take,release)==0);
 assert(mu_stock_reconnect_step(&s,1,0)==CP_WCP_WAIT&&!f.opens);
 f.acl=1; /* original phone auto-connects while USB is still in use */
 assert(mu_stock_reconnect_step(&s,1,1)==CP_WCP_WAIT&&!f.opens);
 f.idle=1; /* USB removed, wait for native transport publication */
 assert(mu_stock_reconnect_step(&s,0,2)==CP_WCP_WAIT&&!f.opens);
 assert(mu_stock_reconnect_step(&s,1,3)==0);
 assert(mu_stock_reconnect_step(&s,1,4003)==CP_WCP_WAIT&&f.opens==1);
 f.idle=0; /* wired again, original priority */
 assert(mu_stock_reconnect_step(&s,1,4004)==0&&f.cancels==1);
 f.idle=1;
 assert(mu_stock_reconnect_step(&s,0,4005)==CP_WCP_WAIT);
 assert(mu_stock_reconnect_step(&s,1,4006)==0);
 assert(mu_stock_reconnect_step(&s,1,8006)==CP_WCP_WAIT&&f.opens==2);
 assert(mu_stock_reconnect_step(&s,0,8007)==0&&f.cancels==2);
 puts("STOCK_AUTO_ACL_REUSE_USB_HANDOFF_PASS_HOST_FIXTURE");
}
