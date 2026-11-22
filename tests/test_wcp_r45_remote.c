/* SPDX-License-Identifier: GPL-3.0-only
 * Synthetic peer on a REAL loopback TCP socket: no car/OEM/MFi success claim.
 * Validates R45/R44 cross-process contract and R87 fail-closed owner gate. */
#include "cp_wcp_r45_remote.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
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
 int alive,published,paired,usb_idle,transport;
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
static int link_owner(void *p,uint32_t e,uint32_t g){return o_owner(p,e,g);}
static int control(void *p,const uint8_t *buf,size_t n){
 (void)p;(void)buf;(void)n;return 0;
}
static int ack(void *p,uint32_t tok){(void)p;(void)tok;return 0;}
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
 o.epoch=8;o.gen=7;o.alive=o.published=o.paired=o.usb_idle=o.transport=1;
 memcpy(o.mac,mac,6);
 s.epoch=o.epoch;
 s.link_owner=(struct cp_lk_owner){&o,link_owner,control,ack};
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
 assert(cp_wcp_bt_tick(&ac,7,103)==CP_WCP_LINK_READY);
 assert(ac.state==CP_WCP_READY);
 assert(out.stock_iap2_connected(out.ctx,6)==0);
 o.paired=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.paired=1;o.usb_idle=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.usb_idle=1;o.transport=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.transport=1;o.published=0;assert(out.stock_iap2_connected(out.ctx,7)==0);
 o.published=1;assert(out.stock_iap2_connected(out.ctx,7)==1);
 assert(cp_wcp_bt_abort(&ac,7,1)==CP_WCP_OK);
 assert(ac.state==CP_WCP_BLOCKED); /* physical USB never starts wireless */
 assert(out.stock_iap2_connected(out.ctx,7)==1); /* still RF, NOT CarPlay */
 /* Fragments of the original iAP marker must traverse the *R44* parser. */
 unsigned char frame[14]={2,0,6,0,7,0,0,0};
 memcpy(frame+8,syn_marker,6);
 assert(send(client,frame,7,0)==7);
 assert(cp_s4_socket_poll(&w,104)==CP_LK_OK);
 assert(send(client,frame+7,7,0)==7);
 assert(cp_s4_socket_poll(&w,105)==CP_LK_OK);
 for(unsigned attempt=0;attempt<20 && s.link.phase==CP_LK_DETECT;attempt++){
  struct pollfd pfd={w.fd,POLLIN,0};
  assert(poll(&pfd,1,100)>=0);
  assert(cp_s4_socket_poll(&w,106+attempt)==CP_LK_OK);
 }
 assert(s.link.phase==CP_LK_NEGOTIATE);
 /* Genuine socket loss: even intact heap data may not authorize session. */
 cp_s4_socket_abort(&w);
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_r45_remote_ops(&r,&out)==-1);
 cp_wcp_r45_remote_unbind(&r);
 close(client);close(listener);
 puts("R89 original btstack->loopback->R44->R87 fail-closed: PASS");
 return 0;
}