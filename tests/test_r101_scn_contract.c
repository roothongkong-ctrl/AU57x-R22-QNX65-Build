/* SPDX-License-Identifier: GPL-3.0-only
 * Real C publication-path check with MOCK vendor functions, not OEM runtime.
 * Valid RFCOMM server channel range is 1..30, matching SDP RFCOMM byte. */
#include "inbound.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static Inbound s;
static uint8_t simulated_channel;
static int add_count;
static int yes(void *x){(void)x;return 1;}
static uint32_t fakeaddr(const void *x){return x?0x13579000u:0;}
static int registration(void *rf,uint8_t *slot,uint16_t hint){
 assert(rf==&s.rf && hint==0);*slot=simulated_channel;return 0;
}
static int unreg(void *rf,uint8_t *slot){assert(rf==&s.rf&&*slot==simulated_channel);return 0;}
static int sdp_add(void *rec){
 assert(rec==&s.sdp || rec==&s.carplay);
 assert(s.protocol[13]==simulated_channel);
 ++add_count;return 0;
}
static int sdp_remove(void *rec){assert(rec==&s.sdp||rec==&s.carplay);return 0;}
static int accept_rf(void *c,int allow){(void)c;(void)allow;return 2;}
static int max_rf(void *c){(void)c;return 256;}
static int send_rf(void *c,void *q){(void)c;(void)q;return 2;}
static int disconnect_rf(void *c){(void)c;return 2;}
static int open_rf(void *c,unsigned mtu){(void)c;(void)mtu;return 1;}
static int close_rf(void *c){(void)c;return 1;}
static int rx_rf(void *c,const uint8_t *b,unsigned len){(void)c;(void)b;(void)len;return 1;}
static int credit_rf(void *c,int n){(void)c;(void)n;return 0;}
static int retire(void *unused,const Inbound *p){(void)unused;return p==&s;}
static void attempt(uint8_t channel,int allowed){
 InboundOps o={0};memset(&s,0,sizeof s);add_count=0;simulated_channel=channel;
 o.reg=registration;o.unreg=unreg;o.add=sdp_add;o.remove=sdp_remove;
 o.accept=accept_rf;o.limit=max_rf;o.send=send_rf;o.disconnect=disconnect_rf;
 o.certify=yes;o.peer_ok=yes;o.ready=yes;o.opened=open_rf;o.closed=close_rf;
 o.rx=rx_rf;o.credit=credit_rf;o.addr=fakeaddr;
 assert(inbound_prepare(&s,&o,0x12345678u)==0);
 if(allowed){
  assert(inbound_publish(&s)==0);
  assert(s.state==IN_PUBLISHED && s.slot==channel);
  assert(s.protocol[13]==channel && add_count==2);
  assert(inbound_withdraw(&s,retire,0)==0 && s.state==IN_RETIRED);
 } else {
  assert(inbound_publish(&s)==-2);
  assert(s.state==IN_DIRTY && s.uncertain_mask==IN_RF);
  assert(add_count==0);
  assert(inbound_withdraw(&s,retire,0)<0);
 }
}
int main(void){
 assert(MU_RFCOMM_SCN_MIN==1 && MU_RFCOMM_SCN_MAX==30);
 attempt(0,0);attempt(1,1);attempt(6,1);attempt(7,1);
 attempt(10,1);attempt(30,1);attempt(31,0);attempt(255,0);
 puts("R101 RFCOMM SCN 1..30, SDP sync, no publish on invalid: PASS (MOCK SDK)");
 return 0;
}
