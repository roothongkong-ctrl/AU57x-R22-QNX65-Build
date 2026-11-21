/* Host-only callback to queued ingress integration; no vendor callbacks. */
#include "cp_mu1005_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int good=1,accept_tx=1,send_count=0;
static int yes(void *p){(void)p;return good;}
static int paired(void*p,uint32_t token,uint8_t mac[6]){
 (void)p;if(token!=0x20000u||!good)return 0;
 uint8_t m[6]={1,2,3,4,5,6};memcpy(mac,m,6);return 1;
}
static int reader(void*p,uint32_t token,uint8_t*out,size_t n){
 (void)p;if(!good||token!=0x30000u||n!=5)return 0;
 memcpy(out,"hello",5);return 1;
}
static int verify_tx(void*p,uint32_t token,uint32_t gen){
 (void)p;return accept_tx&&token==0x70000u&&gen==1;
}
static int send_mock(void*p,const uint8_t*v,size_t n){
 (void)p;(void)v;assert(n==2);send_count++;return 0;
}
static void put16(uint8_t*p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void put32(uint8_t*p,uint32_t n){for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
int main(void){
 static AuRfIngress rf;
 struct cp_mu_bridge bridge;
 const struct cp_mu_bridge_owner ops={{0,yes,paired,reader},verify_tx};
 uint8_t evt[16]={0},rx[8]={0};
 au_rf_init(&rf,send_mock,0);
 assert(cp_mu_bridge_init(&bridge,&rf,2)==CP_MB_OK);
 evt[0]=1;put32(evt+12,0x20000);
 assert(cp_mu_bridge_feed(&bridge,evt,1,&ops,rx,sizeof rx)==CP_MB_STALE);
 assert(cp_mu_bridge_feed(&bridge,evt,2,&ops,rx,sizeof rx)==CP_MB_PENDING);
 assert(cp_mu_bridge_request_decided(&bridge,1,1)==CP_MB_STALE);
 assert(cp_mu_bridge_request_decided(&bridge,2,1)==CP_MB_OK);
 evt[0]=2;
 assert(cp_mu_bridge_feed(&bridge,evt,2,&ops,rx,sizeof rx)==CP_MB_OK);
 assert(rf.connected&&rf.generation==1&&rf.count==1);
 evt[0]=5;put16(evt+8,5);put32(evt+12,0x30000);
 assert(cp_mu_bridge_feed(&bridge,evt,2,&ops,rx,sizeof rx)==CP_MB_OK);
 assert(rf.count==2);
 assert(au_rf_set_max_payload(&rf,2)==0);
 uint8_t packet[]={2,0,2,0,1,0,0,0,'a','b'};
 assert(au_rf_daemon_data(&rf,packet,sizeof packet)==0&&send_count==1);
 evt[0]=6;put16(evt+8,0);put32(evt+12,0x70000);
 accept_tx=0;
 assert(cp_mu_bridge_feed(&bridge,evt,3,&ops,rx,sizeof rx)==CP_MB_STALE);
 accept_tx=1;
 assert(cp_mu_bridge_feed(&bridge,evt,2,&ops,rx,sizeof rx)==CP_MB_OK);
 assert(!rf.tx_busy);
 evt[0]=4;put32(evt+12,0);
 assert(cp_mu_bridge_feed(&bridge,evt,2,&ops,rx,sizeof rx)==CP_MB_OK);
 assert(bridge.closed&&!rf.connected);
 assert(cp_mu_bridge_feed(&bridge,evt,2,&ops,rx,sizeof rx)==CP_MB_BLOCKED);
 puts("AU57X_R39_CALLBACK_TO_RF_QUEUED_BRIDGE_PASS");
 return 0;
}
