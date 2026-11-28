/* SPDX-License-Identifier: GPL-3.0-only */
#include "stock_paired.h"
#include "paired_start.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {MuStockPaired stock;uint32_t generation;unsigned calls,opens,cancels;
 int cert,selected,bond,change,idle;uint8_t mac[6];} F;
static int cert(void*v){return ((F*)v)->cert;}
static int selected(void*v,uint32_t*g,uint8_t m[6]){
 F*f=v;*g=f->generation;memcpy(m,f->mac,6);
 if(f->change&&++f->calls%2==0)m[5]^=1;
 return f->selected;
}
static int readmeta(void*v,uint32_t a,void*out,size_t n){
 F*f=v;uint8_t*b=out;memset(b,0,n);
 if(a==0x2c3d54u&&n==4){b[0]=1;return 1;}
 if(a==0x2c3164u&&n==2){b[0]=1;return 1;}
 if(a==0x2c3168u&&n==8){memcpy(b,f->mac,6);if(!f->bond)b[0]^=1;b[6]=0x20;return 1;}
 assert(!"fixture rejects any non-identity read");return 0;
}
static unsigned take(void*v){(void)v;return 0;}
static int owner(void*v,uint32_t g){F*f=v;return f->cert&&g==f->generation;}
static int idle(void*v){return ((F*)v)->idle;}
static int down(void*v,uint32_t g){(void)v;(void)g;return 0;}
static int open_stock(void*v,uint32_t g){F*f=v;assert(owner(v,g));f->opens++;return CP_WCP_OPEN_PENDING;}
static int cancel(void*v,uint32_t g){(void)g;((F*)v)->cancels++;return 1;}
static int released(void*v,uint32_t g){(void)v;(void)g;return 1;}
int main(void){
 F f={0};f.stock=(MuStockPaired){&f,cert,selected,readmeta};
 f.cert=f.selected=f.bond=1;f.generation=7;f.mac[0]=2;f.mac[5]=5;
 uint32_t g=0;
 assert(mu_stock_paired_snapshot(&f,&g)==1&&g==7);
 f.bond=0;assert(mu_stock_paired_snapshot(&f,&g)==-1&&!g);f.bond=1;
 f.change=1;assert(mu_stock_paired_snapshot(&f,&g)==-1&&!g);f.change=0;
 f.cert=0;assert(mu_stock_paired_snapshot(&f,&g)==-1&&!g);f.cert=1;
 f.selected=0;assert(mu_stock_paired_snapshot(&f,&g)==0&&!g);f.selected=1;
 struct cp_wcp_bt_ops o={&f,owner,owner,idle,down,owner,open_stock,cancel};
 MuPairedStart s;assert(mu_paired_start_init(&s,&o,mu_stock_paired_snapshot,take,released)==0);
 assert(mu_paired_start_tick(&s,0)==CP_WCP_WAIT&&!f.opens);
 f.idle=1;assert(mu_paired_start_tick(&s,1)==0);
 assert(mu_paired_start_tick(&s,4001)==CP_WCP_WAIT&&f.opens==1);
 f.bond=0;assert(mu_paired_start_tick(&s,4002)==0&&f.cancels==1);
 puts("STOCK_BOND_REUSE_NO_DATABASE_PASS_HOST_FIXTURE");
}
