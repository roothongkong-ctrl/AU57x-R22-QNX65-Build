/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only! Synthetic OEM MFi, never a real Apple or vendor authentication.
 * Exercise REAL TCP socket, R44 iAP2, synthetic AA00-AA05, and R87 gate.
 */
#include "cp_wcp_r45_remote.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
struct original {
 int fd;
 uint32_t epoch,gen;
 int alive,published,paired,usb_idle,transport,mfi_peer;
 uint8_t mac[6];
};
static int allow(void *p,uint32_t e){struct original *o=p;return o->alive&&e==o->epoch;}
static int sock_auth(void *p,int fd,uint32_t e){
 struct original *o=p;return o->alive&&o->fd==fd&&e==o->epoch;
}
static int o_owner(void *p,uint32_t e,uint32_t g){
 struct original *o=p;return o->alive&&e==o->epoch&&g==o->gen;
}
static int o_server(void *p,uint32_t e,uint32_t g){
 struct original *o=p;return o->published&&e==o->epoch&&g==o->gen;
}
static int o_paired(void *p,uint32_t e,uint32_t g,const uint8_t mac[6]){
 struct original *o=p;
 return o->paired&&e==o->epoch&&g==o->gen&&memcmp(mac,o->mac,6)==0;
}
static int o_wired(void *p){return ((struct original*)p)->usb_idle;}
static int o_transport(void *p,int fd,uint32_t e,uint32_t g){
 struct original *o=p;
 return o->transport&&fd==o->fd&&e==o->epoch&&g==o->gen;
}
/* Fixture functions are intentionally not real OEM proof. */
static int cert_calls=0,sig_calls=0;
int APSMFiPlatform_CopyCertificate(uint8_t **ptr,size_t *n){
 *ptr=malloc(4);if(!*ptr)return -1;
 memcpy(*ptr,"MOCK",4);*n=4;cert_calls++;return 0;
}
int APSMFiPlatform_CreateSignature(const uint8_t *challenge,size_t len,
                                    uint8_t **ptr,size_t *n){
 if(len!=20)return -1;
 *ptr=malloc(20);if(!*ptr)return -1;
 for(unsigned i=0;i<20;i++)(*ptr)[i]=(uint8_t)(challenge[i]^0x66);
 *n=20;sig_calls++;return 0;
}
static int o_stock_alive(void *p){return ((struct original*)p)->alive;}
static int o_peer_auth(void *p,uint32_t e,uint32_t g){
 struct original *o=p;
 return o->mfi_peer&&o_owner(p,e,g);
}
static int o_other(void *p,const uint8_t *v,size_t n){
 (void)p;(void)v;(void)n;return 1;
}
static void o_free(void *p,void *v){(void)p;free(v);}
static void r90_control_msg(uint8_t *msg,unsigned id,const uint8_t *data,size_t n){
 size_t count=n?n+10u:6u;
 msg[0]=0x40;msg[1]=0x40;
 msg[2]=(uint8_t)(count>>8);msg[3]=(uint8_t)count;
 msg[4]=(uint8_t)(id>>8);msg[5]=(uint8_t)id;
 if(n){msg[6]=0;msg[7]=(uint8_t)(4+n);msg[8]=0;msg[9]=0;memcpy(msg+10,data,n);}
}
static uint8_t r90_sum(const uint8_t *v,size_t n){
 uint8_t s=0;
 for(size_t i=0;i<n;i++)s=(uint8_t)(s+v[i]);
 return (uint8_t)(0u-s);
}
static size_t r90_packet(uint8_t *out,uint8_t control,uint8_t psn,
                         uint8_t ack,uint8_t sid,const uint8_t *payload,size_t n){
 size_t total=n?n+10u:9u;
 out[0]=0xff;out[1]=0x5a;
 out[2]=(uint8_t)(total>>8);out[3]=(uint8_t)total;
 out[4]=control;out[5]=psn;out[6]=ack;out[7]=sid;
 out[8]=r90_sum(out,8);
 if(n){memcpy(out+9,payload,n);out[9+n]=r90_sum(payload,n);}
 return total;
}
static void r90_send_data(int fd,struct cp_s4_socket_io *w,
                          const uint8_t *p,size_t n,uint64_t now){
 uint8_t frame[8+256]={2,0,0,0,7,0,0,0};
 assert(n<=256);
 frame[2]=(uint8_t)n;frame[3]=(uint8_t)(n>>8);
 memcpy(frame+8,p,n);
 /* Break header and payload at independent boundaries to exercise the
  * production stream parser over a real AF_INET TCP connection. */
 assert(send(fd,frame,3,0)==3);
 assert(cp_s4_socket_poll(w,now)==CP_LK_OK);
 assert(send(fd,frame+3,n+5,0)==(ssize_t)(n+5));
 assert(cp_s4_socket_poll(w,now+1)==CP_LK_OK);
}
static void make_pair(int *listener,int *client,int *accepted){
 struct sockaddr_in a;
 *listener=socket(AF_INET,SOCK_STREAM,0);assert(*listener>=0);
 int flag=1;assert(setsockopt(*listener,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof flag)==0);
 memset(&a,0,sizeof a);a.sin_family=AF_INET;
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(CP_S4_LOCAL_PORT);
 assert(bind(*listener,(struct sockaddr*)&a,sizeof a)==0);
 assert(listen(*listener,1)==0);
 *client=socket(AF_INET,SOCK_STREAM,0);assert(*client>=0);
 assert(connect(*client,(struct sockaddr*)&a,sizeof a)==0);
 assert(setsockopt(*client,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof flag)==0);
 *accepted=accept(*listener,NULL,NULL);assert(*accepted>=0);
}
int main(void){
 struct original o={0};
 struct cp_step04_session s={0};
 struct cp_stock_mfi stock={0};
 void *lib=dlopen(NULL,RTLD_NOW);
 uint8_t msg[64],challenge[20];
 assert(lib);
 struct cp_s4_tcp_bridge t;
 struct cp_s4_socket_io w={0};
 struct cp_wcp_r45_remote r={0};
 struct cp_wcp_r45_remote_owner ops;
 struct cp_wcp_bt_ops out;
 struct cp_wcp_autoconnect ac;
 int listener,client,server;
 const uint8_t mac[6]={2,4,6,8,10,12};
 const uint8_t connect_frame[14]={1,0,6,0,7,0,0,0,2,4,6,8,10,12};
 const uint8_t syn_marker[6]={0xff,0x55,2,0,0xee,0x10};
 const uint8_t lsp[]={1,4,0x05,0x80,0x0f,0xa0,0x01,0xf4,4,3,10,0,1};
 uint8_t iap_frame[128];size_t iap_len;
 o.epoch=8;o.gen=7;o.alive=o.published=o.paired=o.usb_idle=o.transport=1;
 memcpy(o.mac,mac,6);
 {
  const struct cp_stock_owner owner={&o,o_owner,o_stock_alive,o_owner,o_wired,o_free};
  const struct cp_s4_external ext={&o,o_owner,o_owner,o_wired,o_peer_auth,o_other};
  assert(cp_s4_init(&s,&stock,o.epoch,&ext)==CP_LK_OK);
  assert(cp_s4_prepare_stock_mfi(&s,lib,&owner)==CP_LK_OK);
  assert(!stock.bound&&!cert_calls&&!sig_calls);
 }
 assert(cp_s4_tcp_init(&t,&s,allow,&o)==CP_LK_OK);
 make_pair(&listener,&client,&server);o.fd=server;
 assert(cp_s4_socket_attach(&w,&t,server,sock_auth,&o)==CP_LK_OK);
 ops=(struct cp_wcp_r45_remote_owner){&o,o_owner,o_server,o_paired,o_wired,o_transport};
 assert(cp_wcp_r45_remote_bind(&r,&w,&t,&ops)==-1); /* no CONNECT */
 /* Fragmented real TCP stream; same R14 normalized framing as R45. */
 assert(send(client,connect_frame,3,0)==3);
 assert(cp_s4_socket_poll(&w,100)==CP_LK_OK);
 assert(cp_wcp_r45_remote_bind(&r,&w,&t,&ops)==-1);
 assert(send(client,connect_frame+3,11,0)==11);
 assert(cp_s4_socket_poll(&w,101)==CP_LK_OK);
 assert(cp_wcp_r45_remote_bind(&r,&w,&t,&ops)==0);
 assert(cp_wcp_r45_remote_ops(&r,&out)==0);
 assert(out.stock_open_iap2(out.ctx,7)<0); /* unverified original client ABI */
 assert(out.phone_iap2_sdp(out.ctx,7)<0);
 assert(cp_wcp_bt_init(&ac,&out)==CP_WCP_OK);
 assert(cp_wcp_bt_acl_up(&ac,7,102)==CP_WCP_OK);
 /* Original R89 bug: a plain R45 CONNECT previously reported LINK_READY
  * even though ZERO iAP2 bytes had arrived. Must remain WAIT. */
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_bt_tick(&ac,7,103)==CP_WCP_WAIT);
 assert(ac.state==CP_WCP_DELAY);
 assert(cp_s4_authenticated(&s)==0);
 /* Marker only: R44 has NOT verified a peer SYN or control session. */
 r90_send_data(client,&w,syn_marker,sizeof syn_marker,104);
 assert(s.link.phase==CP_LK_NEGOTIATE);
 assert(!s.link.have_peer_syn);
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_bt_tick(&ac,7,106)==CP_WCP_WAIT);
 /* Valid LSP/SYN still needs an ACK of our original SYN sequence 99. */
 iap_len=r90_packet(iap_frame,0x80,42,99,0,lsp,sizeof lsp);
 r90_send_data(client,&w,iap_frame,iap_len,107);
 assert(s.link.have_peer_syn&&s.link.phase==CP_LK_NEGOTIATE);
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_bt_tick(&ac,7,109)==CP_WCP_WAIT);
 /* Only a valid peer ACK transitions the R44 iAP2 link to NORMAL. */
 iap_len=r90_packet(iap_frame,0x40,42,99,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,110);
 assert(s.link.phase==CP_LK_NORMAL);
 assert(out.stock_iap2_connected(out.ctx,7)==1);
 assert(cp_wcp_bt_tick(&ac,7,112)==CP_WCP_LINK_READY);
 assert(ac.state==CP_WCP_READY);
 /* Finished iAP2 LINK does not equal original Apple MFi success. */
 assert(cp_s4_authenticated(&s)==0);
 /* The phone must complete AA00 / original certificate and application ACK. */
 r90_control_msg(msg,0xaa00,NULL,0);
 iap_len=r90_packet(iap_frame,0x40,43,99,10,msg,6);
 r90_send_data(client,&w,iap_frame,iap_len,113);
 assert(stock.bound&&stock.bt_generation==t.generation&&stock.epoch==t.epoch);
 assert(!s.pending_stock_library&&cert_calls==1&&s.auth.stage==CP_AUTH_CERT_PENDING);
 puts("PASS deferred OEM MFi binding on negotiated RFCOMM generation (fixture)");
 assert(!cp_s4_authenticated(&s));
 iap_len=r90_packet(iap_frame,0x40,43,100,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,115);
 assert(s.auth.stage==CP_AUTH_AWAIT_CHALLENGE);
 for(unsigned i=0;i<20;i++)challenge[i]=(uint8_t)i;
 r90_control_msg(msg,0xaa02,challenge,20);
 iap_len=r90_packet(iap_frame,0x40,44,100,10,msg,30);
 r90_send_data(client,&w,iap_frame,iap_len,117);
 assert(sig_calls==1&&s.auth.stage==CP_AUTH_SIGNATURE_PENDING);
 assert(!cp_s4_authenticated(&s));
 iap_len=r90_packet(iap_frame,0x40,44,101,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,119);
 assert(s.auth.stage==CP_AUTH_AWAIT_PEER);
 assert(!cp_s4_authenticated(&s));
 o.mfi_peer=1;
 r90_control_msg(msg,0xaa05,NULL,0);
 iap_len=r90_packet(iap_frame,0x40,45,101,10,msg,6);
 r90_send_data(client,&w,iap_frame,iap_len,121);
 assert(cp_s4_authenticated(&s));
 /* Unlike the original R89 test, this is a synthetic OEM certificate/MFi
  * used only to prove C code ordering, NOT an Apple authentication. */
 assert(out.stock_iap2_connected(out.ctx,6)==0);
 o.paired=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.paired=1;o.mfi_peer=0;assert(!cp_s4_authenticated(&s));
 o.mfi_peer=1;assert(!cp_s4_authenticated(&s)); /* owner loss is irreversible */
 o.paired=1;o.usb_idle=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.usb_idle=1;o.transport=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.transport=1;o.published=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.published=1;assert(out.stock_iap2_connected(out.ctx,7)==1);
 assert(cp_wcp_bt_abort(&ac,7,1)==CP_WCP_OK);
 assert(ac.state==CP_WCP_BLOCKED); /* physical USB never starts wireless */
 assert(out.stock_iap2_connected(out.ctx,7)==1); /* iAP2, NOT MFi */
 /* Genuine socket loss: even intact heap data may not authorize session. */
 cp_s4_socket_abort(&w);
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_r45_remote_ops(&r,&out)==-1);
 cp_wcp_r45_remote_unbind(&r);
 close(client);close(listener);dlclose(lib);
 puts("R90 real TCP->R44 iAP2 SYN/LSP/ACK->fixture AA00-AA05->R87: PASS");
 return 0;
}
