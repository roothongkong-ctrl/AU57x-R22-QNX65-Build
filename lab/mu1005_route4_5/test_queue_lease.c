/* SPDX-License-Identifier: GPL-3.0-only */
#include "bt_queue_pulse.h"
#include <assert.h>
#include <stdio.h>
typedef struct{MuBtPulse*p;int available,held,enter,leave,run,disable_during_enter;} F;
static int enter(void*v){F*f=v;assert(!f->held);f->enter++;if(!f->available)return 0;f->held=1;if(f->disable_during_enter)mu_bt_pulse_disable(f->p);return 1;}
static void leave(void*v){F*f=v;assert(f->held);f->held=0;f->leave++;}
static int act(void*v){F*f=v;assert(f->held);f->run++;return 1;}
static int call(void*v){F*f=v;return mu_bt_pulse_dispatch_leased(f->p,enter,leave,f,act,f);}
int main(void){
 MuBtPulse p={0,1};F f={&p,0,0,0,0,0,0};
 assert(mu_bt_pulse_request(&p));assert(!mu_bt_pulse_execute(&p,call,&f));assert(f.enter==1&&!f.run&&!f.leave);mu_bt_pulse_release(&p);
 f.available=1;assert(mu_bt_pulse_request(&p));assert(mu_bt_pulse_execute(&p,call,&f));assert(f.run==1&&f.leave==1&&!f.held);mu_bt_pulse_release(&p);
 f.disable_during_enter=1;assert(mu_bt_pulse_request(&p));assert(!mu_bt_pulse_execute(&p,call,&f));assert(f.run==1&&f.leave==2&&!f.held);mu_bt_pulse_release(&p);
 assert(!mu_bt_pulse_dispatch_leased(&p,enter,leave,&f,act,&f)&&f.enter==3);
 puts("ROUTE6_8_9_DISPATCH_LIFETIME_TRY_LEASE_PASS_FIXTURE_ONLY");return 0;
}
