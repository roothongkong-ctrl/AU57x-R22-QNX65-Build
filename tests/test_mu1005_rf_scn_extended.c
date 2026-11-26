#include "inbound.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned assigned,adds,registrations;
static int yes(void *x){(void)x;return 1;}
static int reg(void *c,uint8_t *scn,uint16_t arg){
 (void)c;assert(arg==0);*scn=(uint8_t)assigned;++registrations;return 0;
}
static int add(void *rec){(void)rec;++adds;return 0;}
static void run(unsigned scn,int pass){
 Inbound s;memset(&s,0,sizeof s);
 assigned=scn;registrations=adds=0;s.state=IN_PREPARED;
 s.ops.certify=yes;s.ops.ready=yes;s.ops.reg=reg;s.ops.add=add;
 int rc=inbound_publish(&s);
 assert(registrations==1);
 if(pass){
  assert(rc==0 && s.state==IN_PUBLISHED && s.slot==scn);
  assert(s.protocol[13]==scn && adds==2);
 }else{
  assert(rc==-2 && s.state==IN_DIRTY && adds==0);
 }
}
int main(void){
 run(1,1);run(6,1);run(7,1);run(30,1);
 run(0,0);run(31,0);
 puts("MU1005 real inbound_publish SCN 1..30: PASS");
 return 0;
}
