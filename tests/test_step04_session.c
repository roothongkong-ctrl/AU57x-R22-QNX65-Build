/* SPDX-License-Identifier: GPL-3.0-only
 * Integration: normalized MU1005 RFCOMM queue -> iAP2 handshake -> OEM MFi
 * certificate/challenge via MOCK symbols -> peer-ACK-proven auth completion.
 * No real radio, original chip, phone or vendor runtime in host test.
 */
#include "cp_step04_session.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct cp_step04_session s;
static AuRfIngress rf;
static struct cp_stock_mfi stock;
static int up=1,control_up=1,peer_ok=0,wired=1;
static int wifi_enabled=0;
static struct cp_wireless wireless;
static struct cp_oem_assertions proof={0,1,1,1,1,0};
static unsigned cert_calls,signature_calls,tx_frames;
int APSMFiPlatform_CopyCertificate(uint8_t **bytes,size_t *len){
 *bytes=malloc(4);if(!*bytes)return -1;
 memcpy(*bytes,"CERT",4);*len=4;cert_calls++;return 0;
}
int APSMFiPlatform_CreateSignature(const uint8_t *challenge,size_t len,
                                   uint8_t **bytes,size_t *out){
 if(len!=20)return -1;
 *bytes=malloc(20);if(!*bytes)return -1;
 for(unsigned i=0;i<20;i++)(*bytes)[i]=(uint8_t)(challenge[i]^0x55);
 *out=20;signature_calls++;return 0;
}
static int bt(void*p,uint32_t e,uint32_t g){(void)p;return up&&e==7&&g==1;}
static int ctl(void*p,uint32_t e,uint32_t g){(void)p;return control_up&&e==7&&g==1;}
static int stock_fingerprint(void*p){(void)p;return up;}
static int wi(void*p){(void)p;return wired;}
static int peer(void*p,uint32_t e,uint32_t g){(void)p;return peer_ok&&e==7&&g==1;}
static int other(void*p,const uint8_t *data,size_t n){
 (void)p;
 if(wifi_enabled){
  int rc=cp_step04_dispatch(&s.auth,&wireless,&proof,data,n);
  if(rc==CP_WL_OK)return 1;
  if(rc!=CP_WL_IGNORED)return 0;
 }
 return 1;
}
static void release(void*p,void*b){(void)p;free(b);}
static int send_native(void*p,const uint8_t *d,size_t n){
 (void)p;assert(d&&n&&n<=128);return 0;
}
static uint8_t checksum(const uint8_t*p,size_t n){
 uint8_t sum=0;for(size_t i=0;i<n;i++)sum=(uint8_t)(sum+p[i]);return (uint8_t)(0u-sum);
}
static size_t frame(uint8_t *dst,unsigned ctrl,unsigned seq,unsigned ack,
                    unsigned sid,const uint8_t *payload,size_t n){
 size_t z=n?n+10:9;
 dst[0]=0xff;dst[1]=0x5a;dst[2]=(uint8_t)(z>>8);dst[3]=(uint8_t)z;
 dst[4]=(uint8_t)ctrl;dst[5]=(uint8_t)seq;dst[6]=(uint8_t)ack;
 dst[7]=(uint8_t)sid;dst[8]=checksum(dst,8);
 if(n){memcpy(dst+9,payload,n);dst[n+9]=checksum(payload,n);}return z;
}
static void feed(const uint8_t *b,size_t n,uint64_t now){
 AuRfWire w;
 assert(au_rf_received(&rf,b,n)==0);
 assert(au_rf_take(&rf,&w)==1);
 assert(cp_s4_accept_rf(&s,&w,now)==CP_LK_OK);
}
static void flush(void){
 unsigned guard=0;
 while(s.link.count){
  assert(++guard<100);
  assert(cp_s4_flush_rf(&s,&rf)==CP_LK_OK);
  for(unsigned i=0;rf.tx_busy&&i<80;i++)assert(au_rf_send_complete(&rf,1)==0);
  assert(!rf.tx_busy);tx_frames++;
 }
}
static void control_msg(uint8_t *dst,unsigned id,const uint8_t *data,size_t n){
 size_t length=n?10+n:6;
 dst[0]=0x40;dst[1]=0x40;dst[2]=(uint8_t)(length>>8);dst[3]=(uint8_t)length;
 dst[4]=(uint8_t)(id>>8);dst[5]=(uint8_t)id;
 if(n){dst[6]=0;dst[7]=(uint8_t)(4+n);dst[8]=0;dst[9]=0;memcpy(dst+10,data,n);}
}
int main(void){
 static const struct cp_stock_owner o={NULL,bt,stock_fingerprint,ctl,wi,release};
 static const struct cp_s4_external ext={NULL,bt,ctl,wi,peer,other};
 uint8_t b[256],message[64];const uint8_t mac[6]={1,2,3,4,5,6};
 void *lib=dlopen(NULL,RTLD_NOW);assert(lib);
 assert(cp_stock_mfi_bind(&stock,lib,&o,7,1)==CP_STOCK_OK);
 assert(cp_s4_init(&s,&stock,7,&ext)==CP_LK_OK);
 au_rf_init(&rf,send_native,NULL);
 assert(au_rf_connected(&rf,mac)==0);
 assert(au_rf_set_max_payload(&rf,128)==0);
 AuRfWire w;assert(au_rf_take(&rf,&w)==1);
 assert(cp_s4_accept_rf(&s,&w,0)==CP_LK_OK);
 flush();
 const uint8_t marker[6]={0xff,0x55,2,0,0xee,0x10};
 feed(marker,6,1);flush();
 const uint8_t lsp[]={1,4,0x05,0x80,0x0f,0xa0,0x01,0xf4,4,3,10,0,1};
 size_t n=frame(b,0x80,42,99,0,lsp,sizeof lsp);
 feed(b,n,2);flush();
 n=frame(b,0x40,42,99,0,NULL,0);
 feed(b,n,3);assert(s.link.phase==CP_LK_NORMAL);
 /* A real stream delivers AA00; not a function call to the isolated auth FSM. */
 control_msg(message,0xaa00,NULL,0);
 n=frame(b,0x40,43,99,10,message,6);
 feed(b,n,4);assert(cert_calls==1);
 assert(s.auth.stage==CP_AUTH_CERT_PENDING&&s.link.outbound_seq==100);
 flush();
 n=frame(b,0x40,43,100,0,NULL,0);
 feed(b,n,5);
 assert(s.auth.stage==CP_AUTH_AWAIT_CHALLENGE);
 uint8_t chal[20];for(int i=0;i<20;i++)chal[i]=(uint8_t)i;
 control_msg(message,0xaa02,chal,sizeof chal);
 n=frame(b,0x40,44,100,10,message,30);
 feed(b,n,6);assert(signature_calls==1);
 assert(s.auth.stage==CP_AUTH_SIGNATURE_PENDING&&s.link.outbound_seq==101);
 flush();
 n=frame(b,0x40,44,101,0,NULL,0);
 feed(b,n,7);assert(s.auth.stage==CP_AUTH_AWAIT_PEER);
 assert(!cp_s4_authenticated(&s));
 peer_ok=1;control_msg(message,0xaa05,NULL,0);
 n=frame(b,0x40,45,101,10,message,6);
 feed(b,n,8);assert(cp_s4_authenticated(&s));
 flush();
 assert(cert_calls==1&&signature_calls==1&&s.auth.stage==CP_AUTH_SUCCEEDED);
 /* A real Bluetooth CONTROL packet 5702 must receive Wi-Fi 5703, then
  * its iAP2 ACK must NOT re-enter the MFi AA01/AA03 acknowledgement FSM. */
 static const struct cp_session config={
  .wifi={.ssid="AUDI_REVIEW",.passphrase="review-password",.security=2,.channel=6}
 };
 cp_wireless_init(&wireless,&config,cp_s4_wireless_send,&s);
 wifi_enabled=1;
 control_msg(message,0x5702,NULL,0);
 n=frame(b,0x40,46,101,10,message,6);
 feed(b,n,9);
 assert(wireless.wifi_pre_sent==1&&s.wireless_pending&&s.link.outbound_pending);
 assert(s.link.outbound_seq==102);
 flush();
 n=frame(b,0x40,46,102,0,NULL,0);
 feed(b,n,10);
 assert(!s.wireless_pending&&!s.link.outbound_pending&&cp_s4_authenticated(&s));
 wifi_enabled=0;
 /* A new generation cannot inherit old auth, and hard owner loss is final. */
 up=0;
 assert(cp_s4_tick(&s,200)==CP_LK_OWNER);
 assert(!cp_s4_authenticated(&s));
 up=1;assert(!cp_s4_authenticated(&s));
 assert(tx_frames>=4);
 dlclose(lib);
 puts("AU57X_R44_IAP2_MFI_STOCK_BYTE_STREAM_INTEGRATION_PASS");return 0;
}
