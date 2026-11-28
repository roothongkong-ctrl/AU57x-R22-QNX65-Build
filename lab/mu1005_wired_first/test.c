/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "wired_first.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
typedef struct {int idle,opens,cancels;uint32_t gen;MuPhoneId usb,acl;char path[240];} F;
static int snap(void*v,uint32_t*g){*g=((F*)v)->gen;return *g!=0;}
static unsigned take(void*v){(void)v;return 0;}
static int owner(void*v,uint32_t g){return ((F*)v)->gen==g;}
static int idle(void*v){return ((F*)v)->idle;}
static int link(void*v,uint32_t g){(void)v;(void)g;return 0;}
static int op(void*v,uint32_t g){F*f=v;assert(owner(v,g));f->opens++;return CP_WCP_OPEN_PENDING;}
static int cancel(void*v,uint32_t g){(void)g;((F*)v)->cancels++;return 1;}
static int released(void*v,uint32_t g){(void)v;(void)g;return 1;}
static int usb(void*v,uint64_t cycle,MuPhoneId*i){*i=((F*)v)->usb;return cycle!=0;}
static int acl(void*v,uint32_t g,MuPhoneId*i){*i=((F*)v)->acl;return owner(v,g);}
static int load(void*v,MuPhoneId*i){return mu_phone_pref_load(((F*)v)->path,i);}
static int save(void*v,const MuPhoneId*i){return mu_phone_pref_save(((F*)v)->path,i);}
int main(void){
 char dir[]="/tmp/mu-wired-first-XXXXXX";assert(mkdtemp(dir));
 F f={0};f.gen=7;f.usb.bytes[0]=1;f.acl=f.usb;
 assert(snprintf(f.path,sizeof f.path,"%s/phone",dir)>0);
 struct cp_wcp_bt_ops b={&f,owner,owner,idle,link,owner,op,cancel};
 MuWiredFirstOps o={&f,usb,acl,load,save};
 MuPairedStart p;MuWiredFirst s;
 assert(mu_paired_start_init(&p,&b,snap,take,released)==0);
 assert(mu_wired_first_init(&s,&p,&o)==0&&!s.valid);
 assert(mu_wired_first_learn(&s,1)==0&&s.valid);
 assert(mu_wired_first_learn(&s,1)==CP_WCP_STALE);
 assert(mu_wired_first_tick(&s,0)==CP_WCP_WAIT&&f.opens==0);
 f.idle=1;f.acl.bytes[0]=2;
 assert(mu_wired_first_tick(&s,1)==CP_WCP_WAIT&&f.opens==0);
 f.acl=f.usb;
 assert(mu_wired_first_tick(&s,2)==0);
 assert(mu_wired_first_tick(&s,4002)==CP_WCP_WAIT&&f.opens==1);
 f.idle=0;
 assert(mu_wired_first_tick(&s,4003)==0&&f.cancels==1);
 f.idle=1;
 assert(mu_wired_first_tick(&s,4004)==0);
 assert(mu_wired_first_tick(&s,8004)==CP_WCP_WAIT&&f.opens==2);
 /* Simulated clean cold boot: only identity persists, no session. */
 assert(mu_paired_start_init(&p,&b,snap,take,released)==0);
 assert(mu_wired_first_init(&s,&p,&o)==0&&s.valid);
 assert(mu_wired_first_tick(&s,0)==0);
 assert(mu_wired_first_tick(&s,4000)==CP_WCP_WAIT&&f.opens==3);
 assert(unlink(f.path)==0&&rmdir(dir)==0);
 puts("WIRED_FIRST_IDENTITY_PERSISTENCE_AND_FRESH_RECONNECT_PASS_HOST_FIXTURE");
 return 0;
}
