/* SPDX-License-Identifier: GPL-3.0-only
 * R46 R14 TCP byte stream -> R44 iAP2/MFi control link, host mocks only.
 * A fake MFi certificate/signature CANNOT authenticate a real iPhone.
 */
#include "cp_step04_tcp_bridge.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct cp_step04_session s;
static struct cp_stock_mfi stock;
static struct cp_s4_tcp_bridge tcp;
static int alive=1,control=1,wired=1,peer_ok=0,socket_ok=1;
static int cert_requests=0,sig_requests=0;
int APSMFiPlatform_CopyCertificate(uint8_t **p,size_t *n){
 *p=malloc(4);if(!*p)return -1;memcpy(*p,"CERT",4);*n=4;cert_requests++;return 0;
}
int APSMFiPlatform_CreateSignature(const uint8_t *c,size_t n,uint8_t **p,size_t *len){
 if(n!=20)return -1;
 *p=malloc(20);
 if(!*p)return -1;
 for(int i=0;i<20;i++)(*p)[i]=(uint8_t)(c[i]^0x66);
 *len=20;sig_requests++;return 0;
}
static int bt(void *p,uint32_t e,uint32_t g){(void)p;return alive&&e==7&&g==1;}
static int ctl(void *p,uint32_t e,uint32_t g){(void)p;return control&&e==7&&g==1;}
static int exists(void*p){(void)p;return alive;}
static int idle(void*p){(void)p;return wired;}
static int peer(void*p,uint32_t e,uint32_t g){(void)p;return peer_ok&&e==7&&g==1;}
static int other(void*p,const uint8_t *v,size_t n){(void)p;(void)v;(void)n;return 1;}
static int sock(void*p,uint32_t e){(void)p;return socket_ok&&e==7;}
static void free_oem(void*p,void*v){(void)p;free(v);}
static uint8_t checksum(const uint8_t *p,size_t n){uint8_t c=0;for(size_t i=0;i<n;i++)c=(uint8_t)(c+p[i]);return (uint8_t)(0u-c);}
static size_t frame(uint8_t *out,unsigned flag,unsigned psn,unsigned ack,unsigned sid,
                    const uint8_t *data,size_t n){
 size_t count=n?n+10u:9u;
 out[0]=0xff;out[1]=0x5a;out[2]=(uint8_t)(count>>8);out[3]=(uint8_t)count;
 out[4]=(uint8_t)flag;out[5]=(uint8_t)psn;out[6]=(uint8_t)ack;
 out[7]=(uint8_t)sid;out[8]=checksum(out,8);
 if(n){memcpy(out+9,data,n);out[9+n]=checksum(data,n);}return count;
}
static size_t envelope(uint8_t *out,unsigned kind,uint32_t gen,const uint8_t *data,size_t n){
 assert(n<=AU_RF_MAX_DATA);out[0]=(uint8_t)kind;out[1]=0;
 out[2]=(uint8_t)n;out[3]=(uint8_t)(n>>8);
 for(unsigned i=0;i<4;i++)out[4+i]=(uint8_t)(gen>>(8*i));
 if(n)memcpy(out+8,data,n);
 return n+8;
}
static void feed(unsigned kind,uint32_t gen,const uint8_t *data,size_t n,uint64_t now){
 uint8_t buf[AU_RF_WIRE_MAX];size_t size=envelope(buf,kind,gen,data,n);
 /* Exact TCP split: mid-header, mid-payload, and full tail. */
 size_t c=size>3?3:size;
 assert(cp_s4_tcp_feed(&tcp,buf,c,now)==CP_LK_OK);
 assert(cp_s4_tcp_feed(&tcp,buf+c,size-c,now)==CP_LK_OK);
}
static int drain(void){
 unsigned attempts=0;const uint8_t *p;size_t n;
 while(1){
  int r=cp_s4_tcp_peek(&tcp,&p,&n);
  assert(r>=0);if(!r)break;
  assert(n>=1&&n<=8+CP_LK_FRAME_MAX);
  assert(++attempts<100);
  if(n>2){
   uint8_t first=p[0];
   assert(cp_s4_tcp_commit(&tcp,2)==CP_LK_OK);
   assert(cp_s4_tcp_peek(&tcp,&p,&n)==1);
   assert(n>0&&first==2);
  }
  assert(cp_s4_tcp_commit(&tcp,n)==CP_LK_OK);
 }
 return attempts;
}
static void control_msg(uint8_t *msg,unsigned id,const uint8_t *data,size_t n){
 size_t length=n?n+10:6;
 msg[0]=0x40;msg[1]=0x40;msg[2]=(uint8_t)(length>>8);msg[3]=(uint8_t)length;
 msg[4]=(uint8_t)(id>>8);msg[5]=(uint8_t)id;
 if(n){msg[6]=0;msg[7]=(uint8_t)(4+n);msg[8]=0;msg[9]=0;memcpy(msg+10,data,n);}
}
int main(void){
 static const struct cp_stock_owner stock_owner={NULL,bt,exists,ctl,idle,free_oem};
 static const struct cp_s4_external ext={NULL,bt,ctl,idle,peer,other};
 const uint8_t mac[6]={1,2,3,4,5,6};
 const uint8_t marker[]={0xff,0x55,2,0,0xee,0x10};
 const uint8_t lsp[]={1,4,0x05,0x80,0x0f,0xa0,0x01,0xf4,4,3,10,0,1};
 uint8_t buf[256],m[64],chal[20]={0};size_t n;
 void *lib=dlopen(NULL,RTLD_NOW);assert(lib);
 assert(cp_stock_mfi_bind(&stock,lib,&stock_owner,7,1)==CP_STOCK_OK);
 assert(cp_s4_init(&s,&stock,7,&ext)==CP_LK_OK);
 socket_ok=0;assert(cp_s4_tcp_init(&tcp,&s,sock,NULL)==CP_LK_OWNER);
 /* Rejecting an unverified socket permanently invalidates this session. */
 assert(s.link.phase==CP_LK_DEAD);
 assert(cp_s4_init(&s,&stock,7,&ext)==CP_LK_OK);
 socket_ok=1;assert(cp_s4_tcp_init(&tcp,&s,sock,NULL)==CP_LK_OK);
 feed(1,1,mac,6,0);assert(tcp.connected&&tcp.generation==1);
 assert(drain()>=1); /* outgoing iAP2 marker */
 feed(2,1,marker,6,1);assert(drain()>=1); /* outgoing SYN */
 n=frame(buf,0x80,42,99,0,lsp,sizeof lsp);feed(2,1,buf,n,2);drain();
 n=frame(buf,0x40,42,99,0,NULL,0);feed(2,1,buf,n,3);
 assert(s.link.phase==CP_LK_NORMAL);
 control_msg(m,0xaa00,NULL,0);
 n=frame(buf,0x40,43,99,10,m,6);feed(2,1,buf,n,4);
 assert(cert_requests==1&&s.auth.stage==CP_AUTH_CERT_PENDING);drain();
 n=frame(buf,0x40,43,100,0,NULL,0);feed(2,1,buf,n,5);
 assert(s.auth.stage==CP_AUTH_AWAIT_CHALLENGE);
 for(unsigned i=0;i<20;i++)chal[i]=(uint8_t)i;
 control_msg(m,0xaa02,chal,20);
 n=frame(buf,0x40,44,100,10,m,30);feed(2,1,buf,n,6);
 assert(sig_requests==1&&s.auth.stage==CP_AUTH_SIGNATURE_PENDING);drain();
 n=frame(buf,0x40,44,101,0,NULL,0);feed(2,1,buf,n,7);
 assert(s.auth.stage==CP_AUTH_AWAIT_PEER&&!cp_s4_authenticated(&s));
 peer_ok=1;control_msg(m,0xaa05,NULL,0);
 n=frame(buf,0x40,45,101,10,m,6);feed(2,1,buf,n,8);
 assert(cp_s4_authenticated(&s));drain();
 uint8_t bad[12];n=envelope(bad,2,2,marker,4);
 assert(cp_s4_tcp_feed(&tcp,bad,n,10)==CP_LK_STALE);
 assert(tcp.closed&&!cp_s4_authenticated(&s));
 assert(cp_s4_tcp_feed(&tcp,bad,n,11)==CP_LK_OWNER);
 dlclose(lib);
 puts("AU57X_R46_R14_TCP_TO_R44_IAP2_STOCK_MFI_PASS");
 return 0;
}
