#define _POSIX_C_SOURCE 200809L
/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only! Synthetic OEM MFi, never a real Apple or vendor authentication.
 * Exercise REAL TCP socket, R44 iAP2, synthetic AA00-AA05, and R87 gate.
 */
#include "cp_wcp_r45_remote.h"
#include "cp_wcp_handoff.h"
#include "bt_iap2_owner_handoff.h"
#include <fcntl.h>
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
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
struct original {
 int fd;
 uint32_t epoch,gen;
 int alive,published,paired,usb_idle,transport,mfi_peer;
 int hotspot,associated,ap_calls;
 int receiver_started,receiver_stopped,bonjour,session,request_ack;
 uint8_t mac[6];
};
static int allow(void *p,uint32_t e){struct original *o=p;return o->alive&&e==o->epoch;}
static int sock_auth(void *p,int fd,uint32_t e){
 struct original *o=p;
 /* Fixture owns the one accepted descriptor; production must identify
  * the original btstack peer independently, not adopt an arbitrary fd. */
 if(o->fd<0&&o->alive&&e==o->epoch)o->fd=fd;
 return o->alive&&o->fd==fd&&e==o->epoch;
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
/* The production pump is nonblocking and a successful send does not
 * guarantee the complete fragment is readable yet. Wait in the test
 * harness only, preserving the production owner tick and logical clock. */
static void await_bytes(int fd,size_t expected){
 struct timespec delay={0,1000000L};
 for(unsigned attempt=0;attempt<1000;attempt++){
  int count=0;
  assert(ioctl(fd,FIONREAD,&count)==0);
  if(count>=0&&(size_t)count>=expected)return;
  nanosleep(&delay,NULL);
 }
 fprintf(stderr,"STEP2_TRANSPORT_TIMEOUT expected=%zu\n",expected);
 abort();
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
 await_bytes(w->fd,3);
 assert(cp_s4_socket_poll(w,now)==CP_LK_OK);
 assert(send(fd,frame+3,n+5,0)==(ssize_t)(n+5));
 await_bytes(w->fd,n+5);
 assert(cp_s4_socket_poll(w,now+1)==CP_LK_OK);
}
static int connect_client(void){
 struct sockaddr_in a;
 int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 int flag=1;
 memset(&a,0,sizeof a);a.sin_family=AF_INET;
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(CP_S4_LOCAL_PORT);
 assert(connect(fd,(struct sockaddr*)&a,sizeof a)==0);
 assert(setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof flag)==0);
 return fd;
}
/* Reuse actual R98 producer-side descriptor ownership instead of creating
 * an unrelated test socket. All OEM ownership providers remain fixtures. */
static int p_alive(void *p){return ((struct original*)p)->alive;}
static int p_service(void *p){return ((struct original*)p)->published;}
static int p_transport(void *p){struct original *o=p;return o->alive&&o->paired&&o->transport&&o->usb_idle;}
static int p_profile(void *p,uint8_t *v){if(!p_transport(p))return 0;*v=4;return 1;}
static int p_read(void *p,uint32_t a,void *b,size_t n){(void)p;(void)a;(void)b;(void)n;return 0;}
static int p_fd(void *p,int fd){return p_transport(p)&&fd>=0&&(fcntl(fd,F_GETFD)&FD_CLOEXEC);}
static int p_attach(void *p,const MuBtOwner *o,int fd){
    assert(p==o->opaque&&o->rf_arg3_snapshot==4&&o->rf_arg3_snapshot_valid);
    return p_fd(p,fd)?0:-1;
}
static int p_tick(void *p){return p_transport(p)?0:-1;}
static int p_barrier(void *p,const Inbound *i){(void)i;return p_alive(p);}
static int p_withdraw(void *p,int(*barrier)(void*,const Inbound*),void *ctx){(void)p;return barrier(ctx,NULL)==1?0:-1;}
/* Step 3 uses the same authenticated step 2 socket and real cloud policy.
 * Only the actual radio/phone evidence providers below are fixture-only. */
static int radio_cap(void *p,enum cp_radio_band b){return p&&b==CP_RADIO_24;}
static int ap_prepare(void *p,uint32_t e,enum cp_radio_band b){
 struct original *o=p;
 if(e!=o->epoch||b!=CP_RADIO_24||!o->mfi_peer)return 0;
 o->ap_calls++;return 1;
}
static int hotspot(void *p,uint32_t e,enum cp_radio_band b){
 struct original *o=p;return e==o->epoch&&b==CP_RADIO_24&&o->hotspot;
}
static int assoc(void *p,uint32_t e,enum cp_radio_band b){
 struct original *o=p;return e==o->epoch&&b==CP_RADIO_24&&o->associated;
}
static int ap_closed(void *p,uint32_t e,enum cp_radio_band b){return !assoc(p,e,b);}
static int epoch_live(void *p,uint32_t e){struct original *o=p;return o->alive&&e==o->epoch;}
static int no_epoch(void *p,uint32_t e){(void)p;(void)e;return 0;}
static int receiver_identity(void *p,uint32_t e,const char *iface){
 struct original *o=p;return e==o->epoch&&o->bonjour&&o->receiver_started&&!strcmp(iface,"wlan0");
}
static int receiver_listener(void *p,uint32_t e,const char *iface,unsigned port){
 struct original *o=p;return e==o->epoch&&o->receiver_started==1&&!strcmp(iface,"wlan0")&&port==5000;
}
static int receiver_session(void *p,uint32_t e,const char *iface,unsigned port){
 return receiver_listener(p,e,iface,port)&&((struct original*)p)->session;
}
static int request_ack(void *p,uint32_t e){struct original *o=p;return e==o->epoch&&o->request_ack;}
/* A real CPRX socket and protocol with deliberately synthetic DIO callbacks.
 * No claimed original libairplay execution, RF, or real phone. */
static int dio_start(void *p,unsigned port){struct original *o=p;return port==5000&&++o->receiver_started==1?2:-1;}
static int dio_poll(void *p){return ((struct original*)p)->receiver_started==1?2:1;}
static int dio_restore(void *p){struct original *o=p;o->receiver_stopped++;o->receiver_started=0;return 0;}
static int dio_admit(void *p,uint32_t g){return epoch_live(p,g);}
static int dio_id(void *p,char *out,size_t n){if(!p||n<4)return -1;memcpy(out,"lab",3);return 3;}
static int dio_tuple(void *p,uint32_t g,uint64_t e,uint32_t s){return g==8&&e==8&&s==1234&&epoch_live(p,g);}
static int dio_peer(void *p,int fd,uint32_t g,uint64_t e,uint32_t s){
 struct sockaddr_un addr; socklen_t n=sizeof addr;
 return dio_tuple(p,g,e,s)&&getpeername(fd,(struct sockaddr*)&addr,&n)==0&&addr.sun_family==AF_UNIX;
}
static int dio_media(void *p){return p?4:-1;}
static int dio_inner(void *p,uint32_t g,uint64_t e,uint32_t s){return dio_tuple(p,g,e,s)?0:-1;}
static int dio_channel(void *p,uint32_t g,uint64_t e,uint32_t s,unsigned ch){(void)p;(void)g;(void)e;(void)s;(void)ch;return -1;}
static void drive_receiver(struct cp_cloud_pipeline *cloud,MuCprxService *rx,uint64_t now){
 enum cp_ho_phase before=cloud->handoff.phase;
 int r=cp_cloud_drive_remote_dio(cloud,now);
 assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
 /* The first client tick can complete op11 and advance the milestone.
  * Do not immediately start another op11: it clears the prior fresh
  * snapshot (remote_inner becomes -1) before its next response. */
 if(cloud->handoff.phase!=before)return;
 assert(mu_cprx_tick(rx,now)==0);
 r=cp_cloud_drive_remote_dio(cloud,now);
 assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
}
static int no_media(void *p,uint32_t e,enum cp_cloud_media m){(void)p;(void)e;(void)m;return 0;}
static void read_exact(int fd,uint8_t *p,size_t n){
 await_bytes(fd,n);
 while(n){ssize_t r=recv(fd,p,n,0);assert(r>0);p+=r;n-=(size_t)r;}
}
/* Parse actual outbound normalized R14 and iAP2 frames; verify the precise
 * expected CSM payload and derive the actual sequence number for peer ACK. */
static uint8_t observe_csm(int fd,const uint8_t *expected,size_t size){
 uint8_t h[8],packet[8192];
 for(unsigned i=0;i<64;i++){
  read_exact(fd,h,sizeof h);
  size_t n=(size_t)h[2]+256u*h[3];
  assert(h[0]==2&&h[1]==0&&h[4]==7&&n>0&&n<=sizeof packet);
  read_exact(fd,packet,n);
  if(n==size+10&&packet[0]==0xff&&packet[1]==0x5a&&
      packet[7]==10&&memcmp(packet+9,expected,size)==0)return packet[5];
 }
 assert(!"expected CSM not received");return 0;
}
static void reject_coalesced_unpaired(void){
 struct original o={0};struct cp_step04_session s={0};struct cp_stock_mfi stock={0};
 struct cp_s4_tcp_bridge t;struct cp_s4_socket_io w={0};struct cp_wcp_r45_remote r={0};
 struct cp_wcp_autoconnect ac={0};
 const uint8_t bytes[]={1,0,6,0,7,0,0,0,2,4,6,8,10,12,
                        2,0,6,0,7,0,0,0,0xff,0x55,2,0,0xee,0x10};
 o.epoch=8;o.gen=7;o.alive=o.published=o.usb_idle=o.transport=1;o.fd=-1;
 memcpy(o.mac,bytes+8,6);
 const struct cp_s4_external ext={&o,o_owner,o_owner,o_wired,o_peer_auth,o_other};
 const struct cp_wcp_r45_remote_owner ops={&o,o_owner,o_server,o_paired,o_wired,o_transport};
 assert(cp_s4_init(&s,&stock,o.epoch,&ext)==CP_LK_OK);
 assert(cp_s4_tcp_init(&t,&s,allow,&o)==CP_LK_OK);
 int l=cp_s4_listener_open();assert(l>=0);
 int fd=connect_client();
 assert(cp_s4_listener_accept(l,&w,&t,sock_auth,&o)==CP_LK_OK);close(l);
 assert(send(fd,bytes,sizeof bytes,0)==sizeof bytes);
 await_bytes(w.fd,sizeof bytes);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,1)==CP_WCP_DENIED);
 assert(!w.attached&&t.closed&&!t.generation&&!r.bound&&!s.auth_ready);
 close(fd);
 puts("PASS coalesced unpaired CONNECT+DATA rejected before iAP2 dispatch");
}
int main(void){
 reject_coalesced_unpaired();
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
 struct cp_wcp_autoconnect ac={0};
 struct cp_cloud_pipeline cloud;
 static const char *const ips[]={"192.0.2.1"};
 static const uint8_t btmac[]={2,4,6,8,10,12};
 static const uint16_t acc[]={0xaa01,0xaa03,0x1d01,0x5703,0x4301};
 static const uint16_t phone[]={0xaa00,0xaa02,0xaa05,0x1d00,0x1d02,0x5702,0x4300};
 static const struct cp_session config={
  .wifi={.ssid="AU57X-LAB",.passphrase="fixture-only",.security=2,.channel=6},
  .ip_addresses=ips,.ip_address_count=1,.airplay_port=5000,
  .public_key="fixture-only",.source_version="fixture-only"};
 static const struct cp_identification identity={
  .name="Review",.model="MU1005",.manufacturer="Audi",.serial="lab",
  .firmware="MU1005",.hardware="lab",.bluetooth_name="lab",
  .wireless_ssid="AU57X-LAB",.bluetooth_mac6=btmac,
  .accessory_messages=acc,.phone_messages=phone,.accessory_message_count=5,.phone_message_count=7};
 const struct cp_cloud_owner cloud_ops={.ctx=&o,.bt_alive=o_owner,.wired_idle=o_wired,
 .radio_capability=radio_cap,.ap_prepare=ap_prepare,.ap_association=assoc,.ap_disconnected=ap_closed,
 .carplay_request_sent=request_ack,.si_single_owner=epoch_live,.bonjour_identity_verified=receiver_identity,
 .airplay_listener_verified=receiver_listener,.inner_iap2_mfi_verified=no_epoch,.media_verified=no_media,
 .teardown_confirmed=epoch_live,.ap_hotspot_verified=hotspot,.airplay_session_connected=receiver_session};
 int listener,client;
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
 cp_cloud_init(&cloud);
 assert(cp_cloud_bind_bootstrap(&cloud,&s,&config,&identity)==CP_CLOUD_OK);
 assert(cp_s4_tcp_init(&t,&s,allow,&o)==CP_LK_OK);
 o.fd=-1;
 listener=cp_s4_listener_open();assert(listener>=0);
 assert(cp_s4_listener_accept(listener,&w,&t,sock_auth,&o)==CP_LK_BACKPRESSURE);
 Au98Handoff handoff={0};
 Au98Hooks hooks={.opaque=&o,.original_image_verified=p_alive,.on_original_iap2_owner=p_alive,.original_iap2_service_verified=p_service,.attach=p_attach,.tick=p_tick,.withdraw=p_withdraw};
 MuBtOwner native={.opaque=&o,.owner_ready=p_alive,.transport_ready=p_transport,.iap2_profile_proven=p_profile,.read_live=p_read};
 client=au98_prepare_daemon_socket(CP_S4_LOCAL_PORT);assert(client>=0);
 {int one=1;assert(setsockopt(client,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one)==0);}
 o.paired=0;
 assert(au98_adopt(&handoff,&hooks,&native,client,CP_S4_LOCAL_PORT,p_fd,&o)==-1);
 o.paired=1;
 assert(au98_adopt(&handoff,&hooks,&native,client,CP_S4_LOCAL_PORT,p_fd,&o)==0);
 assert(handoff.fd==client&&handoff.state==AU98_PUBLISHED);
 assert(au98_tick(&handoff)==0);
 puts("STEP2 same actual producer socket adopted by R98");
 assert(cp_s4_listener_accept(listener,&w,&t,sock_auth,&o)==CP_LK_OK);
 assert(w.fd==o.fd&&w.attached);
 close(listener);listener=-1;
 puts("PASS real single-consumer 57342 listener and accepted RFCOMM transport (fixture)");
 ops=(struct cp_wcp_r45_remote_owner){&o,o_owner,o_server,o_paired,o_wired,o_transport};
 assert(cp_wcp_r45_remote_bind(&r,&w,&t,&ops)==-1); /* no CONNECT */
 /* Fragmented real TCP stream; same R14 normalized framing as R45. */
 assert(send(client,connect_frame,3,0)==3);
 await_bytes(w.fd,3);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,100)==CP_WCP_WAIT&&!r.bound);
 assert(send(client,connect_frame+3,11,0)==11);
 await_bytes(w.fd,11);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,101)==CP_WCP_WAIT&&r.bound);
 assert(ac.generation==7&&ac.state==CP_WCP_DELAY);
 assert(cp_wcp_r45_remote_ops(&r,&out)==0);
 assert(out.stock_open_iap2(out.ctx,7)<0); /* unverified original client ABI */
 assert(out.phone_iap2_sdp(out.ctx,7)<0);
 /* Original R89 bug: a plain R45 CONNECT previously reported LINK_READY
  * even though ZERO iAP2 bytes had arrived. Must remain WAIT. */
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,103)==CP_WCP_WAIT);
 assert(ac.state==CP_WCP_DELAY);
 assert(cp_s4_authenticated(&s)==0);
 /* Marker only: R44 has NOT verified a peer SYN or control session. */
 r90_send_data(client,&w,syn_marker,sizeof syn_marker,104);
 assert(s.link.phase==CP_LK_NEGOTIATE);
 assert(!s.link.have_peer_syn);
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,106)==CP_WCP_WAIT);
 /* Valid LSP/SYN still needs an ACK of our original SYN sequence 99. */
 iap_len=r90_packet(iap_frame,0x80,42,99,0,lsp,sizeof lsp);
 r90_send_data(client,&w,iap_frame,iap_len,107);
 assert(s.link.have_peer_syn&&s.link.phase==CP_LK_NEGOTIATE);
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,109)==CP_WCP_WAIT);
 /* Only a valid peer ACK transitions the R44 iAP2 link to NORMAL. */
 iap_len=r90_packet(iap_frame,0x40,42,99,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,110);
 assert(s.link.phase==CP_LK_NORMAL);
 assert(out.stock_iap2_connected(out.ctx,7)==1);
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,112)==CP_WCP_WAIT);
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
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,123)==CP_WCP_OK);
 puts("PASS unified TCP owner tick reaches authenticated bootstrap handoff (fixture)");

 /* Do not create a second iAP2 link: use the exact session authenticated above. */
 assert(cp_wcp_begin_cloud(&ac,&s,&cloud,&cloud_ops)==CP_WCP_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&cloud)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&cloud)==CP_CLOUD_OK&&cloud.band==CP_RADIO_24&&o.ap_calls==1);
 assert(cp_cloud_radio_result(&cloud,1,0)==CP_CLOUD_PROOF);
 uint8_t expected[CP_LK_FRAME_MAX];size_t size=0;uint8_t seq;
 r90_control_msg(msg,0x1d00,NULL,0);
 iap_len=r90_packet(iap_frame,0x40,46,101,10,msg,6);
 r90_send_data(client,&w,iap_frame,iap_len,125);
 assert(cp_encode_identification_1d01(&identity,expected,sizeof expected,&size)==CP_OK);
 seq=observe_csm(client,expected,size);
 iap_len=r90_packet(iap_frame,0x40,46,seq,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,127);
 r90_control_msg(msg,0x1d02,NULL,0);
 iap_len=r90_packet(iap_frame,0x40,47,seq,10,msg,6);
 r90_send_data(client,&w,iap_frame,iap_len,129);
 assert(s.identity_accepted);
 r90_control_msg(msg,0x5702,NULL,0);
 iap_len=r90_packet(iap_frame,0x40,48,seq,10,msg,6);
 r90_send_data(client,&w,iap_frame,iap_len,131);
 assert(s.wifi_request_pending&&!s.wireless_pending);
 o.hotspot=1;
 assert(cp_s4_wireless_pump(&s)==CP_WL_OK);
 assert(cp_s4_socket_poll(&w,133)==CP_LK_OK);
 assert(cp_encode_wifi_5703(&config.wifi,expected,sizeof expected,&size)==CP_OK);
 seq=observe_csm(client,expected,size);
 assert(s.wireless_pending);
 iap_len=r90_packet(iap_frame,0x40,48,seq,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,134);
 assert(!s.wireless_pending&&s.wireless.wifi_pre_sent==1);
 assert(cp_cloud_radio_result(&cloud,1,0)==CP_CLOUD_PROOF);
 o.associated=1;
 assert(cp_cloud_radio_result(&cloud,1,0)==CP_CLOUD_OK);
 assert(cloud.radio.phase==CP_RADIO_ASSOCIATED&&cloud.handoff.phase==CP_HO_WIFI_ASSOCIATED);
 puts("STEP3_SAME_IAP2_SOCKET_5703_PEER_ACK_AND_24GHZ_ASSOCIATION_FIXTURE_PASS");
 assert(!s.wireless.start_sent);
 /* Continue through the existing remote SI/DIO receiver interface. */
 char dir[]="/tmp/au57x-step4-XXXXXX";assert(mkdtemp(dir));
 MuCprxService rx={0};
 MuReceiverBackend backend={&o,8,dio_start,dio_poll,dio_restore};
 MuCprxEvidence evidence={.opaque=&o,.admitted=dio_admit,.identity=dio_id,.media=dio_media,
 .si_admitted=dio_tuple,.si_client_verified=dio_peer,.si_inner_iap2_mfi=dio_inner,.si_media_channel=dio_channel};
 assert(mu_cprx_open_backend_si_unix(&rx,&backend,&evidence,5000,8,1234,dir,geteuid(),getegid())==0);
 MuCprxRemoteEndpoint ep={.generation=8,.epoch=8,.session=1234,.airplay_port=5000,
 .server_uid=geteuid(),.server_gid=getegid(),.owner_ctx=&o,.si_owner_verified=dio_tuple,.dio_server_verified=dio_peer};
 assert(strlen(rx.unix_path)<sizeof ep.socket_path);strcpy(ep.socket_path,rx.unix_path);
 assert(cp_cloud_bind_remote_dio(&cloud,&ep,"wlan0")==CP_CLOUD_OK);
 uint64_t clock=140;
 for(unsigned i=0;i<100&&!cloud.remote_start_confirmed;i++)drive_receiver(&cloud,&rx,clock++);
 assert(cloud.remote_start_confirmed&&o.receiver_started==1);
 /* Availability arrives over the same authenticated TCP as 5703. */
 static const uint8_t offer[]={0x40,0x40,0,15,0x43,0,0,9,0,1,0,5,0,0,1};
 iap_len=r90_packet(iap_frame,0x40,49,seq,10,offer,sizeof offer);
 r90_send_data(client,&w,iap_frame,iap_len,clock);clock+=2;
 assert(s.wireless.availability_seen&&!s.wireless.start_sent);
 o.bonjour=1;
 assert(cp_s4_wireless_pump(&s)==CP_WL_OK);
 assert(cp_s4_socket_poll(&w,clock++)==CP_LK_OK);
 assert(cp_encode_start_4301(&config,expected,sizeof expected,&size)==CP_OK);
 seq=observe_csm(client,expected,size);
 assert(s.wireless_pending&&cloud.handoff.phase==CP_HO_WIFI_ASSOCIATED);
 for(unsigned i=0;i<20;i++)drive_receiver(&cloud,&rx,clock++);
 assert(cloud.handoff.phase==CP_HO_WIFI_ASSOCIATED);
 iap_len=r90_packet(iap_frame,0x40,49,seq,0,NULL,0);
 r90_send_data(client,&w,iap_frame,iap_len,clock);clock+=2;
 assert(!s.wireless_pending);
 o.request_ack=1;
 for(unsigned i=0;i<100&&cloud.handoff.phase!=CP_HO_START_REQUESTED;i++)drive_receiver(&cloud,&rx,clock++);
 assert(cloud.handoff.phase==CP_HO_START_REQUESTED&&!cloud.remote_bootstrap_committed);
 o.session=1;
 for(unsigned i=0;i<100&&cloud.handoff.phase!=CP_HO_AIRPLAY_SESSION;i++)drive_receiver(&cloud,&rx,clock++);
 fprintf(stderr,"STEP4_SESSION_OBSERVATION phase=%d inner=%d fresh=%d media=%d channels=%u\n",cloud.handoff.phase,cloud.remote_inner,cloud.remote_snapshot_fresh,cloud.remote_media,cloud.remote_channels);
 assert(cloud.handoff.phase==CP_HO_AIRPLAY_SESSION&&cloud.remote_snapshot_fresh&&cloud.remote_inner==0&&cloud.remote_media==4&&!cloud.remote_channels);
 assert(!cloud.remote_bootstrap_committed&&!cp_cloud_remote_video_at(&cloud,clock));
 puts("STEP4_REAL_CPRX_AND_4301_ACK_SESSION_FIXTURE_PASS_NO_OEM");
 /* No active media: close test client and restore receiver fixture. */
 mu_cprx_client_close(&cloud.remote_client);
 assert(mu_cprx_shutdown(&rx)==1&&o.receiver_stopped==1);
 assert(rmdir(dir)==0);


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
 o.paired=0;
 assert(cp_wcp_r45_bootstrap_tick(&r,&w,&t,&ops,&ac,1000)==CP_WCP_DENIED);
 assert(!w.attached&&t.closed&&ac.state==CP_WCP_BLOCKED);
 puts("PASS established RFCOMM owner revoked before subsequent iAP2 socket pump");
 assert(out.stock_iap2_connected(out.ctx,7)==0);
 assert(cp_wcp_r45_remote_ops(&r,&out)==-1);
 cp_wcp_r45_remote_unbind(&r);
 assert(au98_tick(&handoff)==-2&&handoff.state==AU98_DRAINING);
 assert(au98_withdraw(&handoff,p_barrier,&o)==0&&handoff.fd==-1);
 dlclose(lib);
 puts("STEP2_INTEGRATED_R98_R45_IAP2_FIXTURE_PASS_NO_REAL_MFI_OR_RF");
 puts("R90 real TCP->R44 iAP2 SYN/LSP/ACK->fixture AA00-AA05->R87: PASS");
 return 0;
}
