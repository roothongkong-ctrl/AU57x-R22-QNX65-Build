#include "bt_queue_pulse.h"
#include <assert.h>
#include <stdio.h>
static int action(void*p){++*(unsigned*)p;return 1;}
int main(void){
 MuBtPulse p={0,1};unsigned n=0;
 for(unsigned i=0;i<100;i++){
  assert(mu_bt_pulse_request(&p)==1);
  for(int j=0;j<20;j++)assert(mu_bt_pulse_request(&p)==0);
  assert(mu_bt_pulse_execute(&p,action,&n)==1);
  assert(mu_bt_pulse_execute(&p,action,&n)==0);
  assert(!mu_bt_pulse_idle(&p)&&!mu_bt_pulse_request(&p));
  mu_bt_pulse_release(&p);assert(mu_bt_pulse_idle(&p));
 }
 assert(n==100);assert(mu_bt_pulse_request(&p));mu_bt_pulse_release(&p);
 assert(mu_bt_pulse_request(&p));mu_bt_pulse_disable(&p);
 assert(!mu_bt_pulse_execute(&p,action,&n)&&n==100);
 mu_bt_pulse_release(&p);assert(mu_bt_pulse_idle(&p)&&!mu_bt_pulse_request(&p));
 assert(mu_bt_queue_pulse_start(NULL)==-1&&mu_bt_queue_pulse_stop()==-1&&mu_bt_queue_pulse_retire()==-1);
 puts("PASS periodic queue event coalescing, release, and stop (host state only)");
}
