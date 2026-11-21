/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only, owner-attested RFCOMM->iAP2 byte stream and reliable control.
 * Fake peer; never genuine BlueSDK or Apple MFi.
 */
#include "cp_step04_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct cp_step04_link link;
static AuRfIngress rf;
static int live=1,control_count=0,ack_count=0,sent_bytes=0;
static uint32_t ack_token=0;
static int owner(void*p,uint32_t e,uint32_t g){(void)p;return live&&e==7&&g==1;}
static uint8_t sum(const uint8_t *p,size_t n){unsigned x=0;for(size_t i=0;i<n;i++)x=(x+p[i])&255u;return (uint8_t)(0u-x);}
static size_t packet(uint8_t*out,unsigned control,unsigned seq,unsigned ack,
                     unsigned sid,const uint8_t *data,size_t n){
 size_t len=n?n+10u:9u;
 out[0]=0xff;out[1]=0x5a;out[2]=(uint8_t)(len>>8);out[3]=(uint8_t)len;
 out[4]=(uint8_t)control;out[5]=(uint8_t)seq;out[6]=(uint8_t)ack;
 out[7]=(uint8_t)sid;out[8]=sum(out,8);
 if(n){memcpy(out+9,data,n);out[9+n]=sum(data,n);}return len;
}
static int on_control(void*p,const uint8_t*data,size_t n){
 (void)p;assert(n>=6&&data[4]==0xaa&&data[5]==0);control_count++;
 const uint8_t reply[]={0x40,0x40,0,6,0xaa,1};
 return cp_lk_queue_control(&link,reply,sizeof reply,11,250)==CP_LK_OK?0:-1;
}
static int on_ack(void*p,uint32_t tok){(void)p;ack_count++;ack_token=tok;return tok==11?0:-1;}
static int sdk_send(void*p,const uint8_t *data,size_t n){(void)p;assert(data&&n&&n<=100);sent_bytes+=(int)n;return 0;}
static void flush_tx(void){
 while(link.count){
  assert(cp_lk_drain_rf(&link,&rf)==CP_LK_OK);
  for(int i=0;rf.tx_busy&&i<100;i++)assert(au_rf_send_complete(&rf,1)==0);
  assert(!rf.tx_busy);
 }
}
static void send_data(const uint8_t *data,size_t n,uint64_t now){
 AuRfWire wire;
 assert(au_rf_received(&rf,data,n)==0);
 assert(au_rf_take(&rf,&wire)==1);
 assert(cp_lk_accept_rf(&link,&wire,7,NULL,now)==CP_LK_OK);
}
int main(void){
 static const struct cp_lk_owner io={NULL,owner,on_control,on_ack};
 uint8_t b[256];const uint8_t peer[6]={1,2,3,4,5,6};
 cp_lk_init(&link);au_rf_init(&rf,sdk_send,NULL);
 assert(au_rf_connected(&rf,peer)==0);
 assert(au_rf_set_max_payload(&rf,100)==0);
 AuRfWire wire;assert(au_rf_take(&rf,&wire)==1);
 assert(cp_lk_accept_rf(&link,&wire,7,&io,0)==CP_LK_OK);
 assert(link.phase==CP_LK_DETECT&&link.count==1);
 flush_tx();assert(sent_bytes==6);
 const uint8_t marker[6]={0xff,0x55,0x02,0,0xee,0x10};
 send_data(marker,2,4);assert(link.phase==CP_LK_DETECT);
 send_data(marker+2,4,5);assert(link.phase==CP_LK_NEGOTIATE);
 flush_tx();
 const uint8_t lsp[]={1,4,0x05,0x80,0x0f,0xa0,0x01,0xf4,4,3,10,0,1};
 size_t n=packet(b,0x80,42,99,0,lsp,sizeof lsp);
 send_data(b,n,100);assert(link.have_peer_syn&&link.rx_psn==42);
 flush_tx();
 n=packet(b,0x40,42,99,0,NULL,0);
 send_data(b,n,101);assert(link.phase==CP_LK_NORMAL);
 const uint8_t csm[]={0x40,0x40,0,6,0xaa,0};
 n=packet(b,0x40,43,99,10,csm,sizeof csm);
 send_data(b,n/2,240);send_data(b+n/2,n-n/2,241);
 assert(control_count==1&&link.outbound_pending&&link.outbound_seq==100);
 flush_tx();
 /* Repeated old PSN must not re-enter MFi. */
 send_data(b,n,242);assert(control_count==1);flush_tx();
 assert(cp_lk_tick(&link,4250)==CP_LK_OK);
 assert(link.outbound_pending&&link.retries==1);flush_tx();
 /* Exact ACK of reliable app token; stale ack never accepted twice. */
 n=packet(b,0x40,43,100,0,NULL,0);
 send_data(b,n,260);assert(ack_count==1&&ack_token==11);
 send_data(b,n,261);assert(ack_count==1);
 flush_tx();
 assert(!link.outbound_pending);
 /* No true OEM owner -> irrevocable dead, not fake authentication. */
 live=0;assert(cp_lk_tick(&link,500)==CP_LK_OWNER);
 assert(link.phase==CP_LK_DEAD);
 assert(cp_lk_queue_control(&link,csm,sizeof csm,12,501)==CP_LK_OWNER);
 puts("AU57X_R43_IAP2_LINK_RFCOMM_STREAM_PASS");return 0;
}
