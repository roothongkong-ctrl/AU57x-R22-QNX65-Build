/* SPDX-License-Identifier: GPL-3.0-only
 * Real loopback TCP round-trip, never a real BlueSDK or iPhone authentication.
 */
#include "cp_step04_socket_io.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
static int accepted_fd=-1,peer_is_authorized=1;
static int live(void *ctx,uint32_t e,uint32_t g){
 (void)ctx;return e==7&&g==1;
}
static int control(void *ctx,const uint8_t *p,size_t n){
 (void)ctx;(void)p;(void)n;return 0;
}
static int acked(void *ctx,uint32_t token){
 (void)ctx;(void)token;return 0;
}
static int peer(void *ctx,uint32_t epoch){
 (void)ctx;return epoch==7&&peer_is_authorized;
}
static int attest(void *ctx,int fd,uint32_t epoch){
 (void)ctx;return peer_is_authorized&&fd==accepted_fd&&epoch==7;
}
static void exact_read(int fd,uint8_t *out,size_t n){
 size_t used=0;
 while(used<n){
  ssize_t got=recv(fd,out+used,n-used,0);
  assert(got>0);
  used+=(size_t)got;
 }
}
static int make_pair(int *client){
 int listener=socket(AF_INET,SOCK_STREAM,0);
 struct sockaddr_in sa;
 struct timeval timeout={3,0};
 int yes=1;
 assert(listener>=0);
 assert(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)==0);
 memset(&sa,0,sizeof sa);
 sa.sin_family=AF_INET;
 sa.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 sa.sin_port=htons(CP_S4_LOCAL_PORT);
 assert(bind(listener,(struct sockaddr *)&sa,sizeof sa)==0);
 assert(listen(listener,1)==0);
 *client=socket(AF_INET,SOCK_STREAM,0);
 assert(*client>=0);
 assert(setsockopt(*client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout)==0);
 assert(connect(*client,(struct sockaddr *)&sa,sizeof sa)==0);
 accepted_fd=accept(listener,NULL,NULL);
 assert(accepted_fd>=0);
 return listener;
}
int main(void){
 struct cp_step04_session session;
 struct cp_s4_tcp_bridge bridge;
 struct cp_s4_socket_io worker={0};
 uint8_t received[64];
 const uint8_t connect_frame[14]={1,0,6,0,1,0,0,0,1,2,3,4,5,6};
 const uint8_t link_marker[6]={0xff,0x55,2,0,0xee,0x10};
 uint8_t data_frame[14]={2,0,6,0,1,0,0,0,0};
 int listener,client;
 memset(&session,0,sizeof session);
 session.epoch=7;
 session.link_owner.live=live;
 session.link_owner.control=control;
 session.link_owner.acked=acked;
 assert(cp_s4_tcp_init(&bridge,&session,peer,NULL)==CP_LK_OK);
 listener=make_pair(&client);
 assert(cp_s4_socket_attach(&worker,&bridge,accepted_fd,attest,NULL)==CP_LK_OK);
 assert(cp_s4_socket_poll(&worker,0)==CP_LK_OK); /* idle accepted socket must survive */
 assert(worker.attached&&!bridge.connected);
 assert(send(client,connect_frame,sizeof connect_frame,0)==(ssize_t)sizeof connect_frame);
 assert(cp_s4_socket_poll(&worker,0)==CP_LK_OK);
 assert(bridge.connected&&bridge.generation==1);
 exact_read(client,received,14);
 assert(received[0]==2&&received[1]==0&&received[2]==6&&received[3]==0);
 assert(memcmp(received+8,link_marker,6)==0);
 memcpy(data_frame+8,link_marker,6);
 assert(send(client,data_frame,sizeof data_frame,0)==(ssize_t)sizeof data_frame);
 assert(cp_s4_socket_poll(&worker,1)==CP_LK_OK);
 exact_read(client,received,31); /* 8-byte R14 header + 23-byte iAP2 SYN */
 assert(received[0]==2&&received[1]==0&&received[2]==23&&received[3]==0);
 assert(received[8]==0xff&&received[9]==0x5a&&received[12]==0x80);
 peer_is_authorized=0;
 assert(cp_s4_socket_poll(&worker,2)==CP_LK_OWNER);
 assert(bridge.closed&&session.link.phase==CP_LK_DEAD);
 assert(!worker.attached);
 close(client);close(listener);
 puts("AU57X_R59_REAL_LOOPBACK_SOCKET_IO_REVIEW_ONLY_PASS");
 return 0;
}
