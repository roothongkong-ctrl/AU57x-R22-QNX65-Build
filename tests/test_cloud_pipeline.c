/* AU57x owner-bound R04-09 cloud integration fixture: no OEM keys or RF. */
#include "cp_cloud_pipeline.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
static int wired=1,bt=1,cap5=1,cap24=1,ap=1,associated=1;
static int single=1,bonjour=1,listener=1,inner=1,request=1,phone_session=1;
static int media[5]={0,1,1,1,1};
static unsigned ap_calls=0,observed_port=0,native_start_calls=0;
static int airplay_object,airplay_owner,airplay_start_key,airplay_stop_key,airplay_interface_key;
static int owner(void*ctx,uint32_t e,uint32_t g){(void)ctx;return bt&&e==8&&g==4;}
static int ack_wireless(void*ctx,uint32_t token){
 struct cp_step04_session *s=ctx;
 if(!s||!s->wireless_pending||s->wireless_token!=token)return -1;
 s->wireless_pending=0;s->wireless_token=0;return 0;
}
static uint8_t sum8(const uint8_t*p,size_t n){
 uint8_t x=0;for(size_t i=0;i<n;i++)x=(uint8_t)(x+p[i]);
 return (uint8_t)(0u-x);
}
static int stock_owner(void*ctx){(void)ctx;return 1;}
static void release_oem(void*ctx,void*ptr){(void)ctx;(void)ptr;}
static int wd(void*ctx){(void)ctx;return wired;}
static int cert_live(void*ctx,uint32_t e,uint32_t g){(void)ctx;return wired&&e==8&&g==4;}
static int peer(void*ctx,uint32_t e,uint32_t g){(void)ctx;return wired&&e==8&&g==4;}
static int caps(void*ctx,enum cp_radio_band b){(void)ctx;return b==CP_RADIO_5?cap5:cap24;}
static int ap_start(void*ctx,uint32_t e,enum cp_radio_band b){
 (void)ctx;assert(e==8);assert(b==CP_RADIO_5||b==CP_RADIO_24);ap_calls++;return ap;
}
static int ap_ok(void*ctx,uint32_t e,enum cp_radio_band b){
 (void)ctx;return associated&&e==8&&(b==CP_RADIO_5||b==CP_RADIO_24);
}
static int ap_closed(void*ctx,uint32_t e,enum cp_radio_band b){
 (void)ctx;return e==8&&(b==CP_RADIO_5||b==CP_RADIO_24);
}
static int running_ap(void*ctx,uint32_t e,enum cp_radio_band b){
 (void)ctx;return ap&&e==8&&(b==CP_RADIO_24||b==CP_RADIO_5);
}
static int cp_start(void*ctx,uint32_t e){(void)ctx;return request&&e==8;}
static int si(void*ctx,uint32_t e){(void)ctx;return single&&e==8;}
static int bonjour_ok(void*ctx,uint32_t e,const char*name){
 (void)ctx;return bonjour&&e==8&&!strcmp(name,"wlan0");
}
static int airplay_ok(void*ctx,uint32_t e,const char*name,unsigned port){
 (void)ctx;observed_port=port;
 return listener&&e==8&&!strcmp(name,"wlan0")&&port==5000;
}
static int session_connected(void*ctx,uint32_t e,const char *name,unsigned port){
 (void)ctx;return phone_session&&e==8&&port==5000&&!strcmp(name,"wlan0");
}
static int inner_ok(void*ctx,uint32_t e){(void)ctx;return inner&&e==8;}
static int media_ok(void*ctx,uint32_t e,enum cp_cloud_media id){
 (void)ctx;return e==8&&id>=CP_CLOUD_VIDEO&&id<=CP_CLOUD_STEERING&&media[id];
}
static int torn(void*ctx,uint32_t e){(void)ctx;return e==8;}
static int cf_ok(void*ctx,const void*p){(void)ctx;return p!=NULL;}
static int cf_string(void*ctx,const void*cf,char*out,size_t cap){
 (void)ctx;(void)cf;const char*name="wlan0";
 if(cap<6)return -1;
 memcpy(out,name,6);return 5;
}
static int setprop(void*ctx,void*server,const void*key,const void*val){
 (void)ctx;return (server==&airplay_object&&key==&airplay_interface_key&&val)?0:-1;
}
static int control_ap(void*ctx,void*server,const void*key,const void*ownerptr){
 (void)ctx;
 if(server!=&airplay_object||ownerptr!=&airplay_owner)return -1;
 if(key==&airplay_start_key){native_start_calls++;return 0;}
 return key==&airplay_stop_key?0:-1;
}
static int confirmed_ap(void*ctx,void*server,const void*ownerptr,unsigned port){
 (void)ctx;return listener&&server==&airplay_object&&ownerptr==&airplay_owner&&port==5000;
}
static void native_airplay(Mu34ApServer *ap){
 const Mu34ApServerBinding bind={stock_owner,cf_ok,cf_string,stock_owner,
  setprop,control_ap,confirmed_ap,NULL,&airplay_object,&airplay_owner,
  &airplay_interface_key,&airplay_start_key,&airplay_stop_key,8};
 assert(mu34_ap_server_init(ap,&bind)==0);
}
static const struct cp_cloud_owner ops={0,owner,wd,caps,ap_start,ap_ok,
 ap_closed,cp_start,si,bonjour_ok,airplay_ok,inner_ok,media_ok,torn,running_ap,
 session_connected};
static void mocked_oem(struct cp_step04_session *s,struct cp_stock_mfi *stock,int authenticated){
 static int handle;
 memset(s,0,sizeof *s);memset(stock,0,sizeof *stock);
 s->epoch=8;s->link.generation=4;s->link.phase=CP_LK_NORMAL;
 s->stock=stock;s->auth_ready=1;
 stock->epoch=8;stock->bt_generation=4;stock->bound=1;
 stock->existing_library=&handle;
 stock->owner.owner_verified=owner;
 stock->owner.stock_image_verified=stock_owner;
 stock->owner.control_session_live=cert_live;
 stock->owner.wired_idle=wd;
 /* Release callback only checked for non-NULL by stock MFi live predicate. */
 stock->owner.release_oem=release_oem;
 s->auth.stock=stock;s->auth.epoch=8;s->auth.bt_generation=4;
 s->auth.stage=authenticated?CP_AUTH_SUCCEEDED:CP_AUTH_AWAIT_CERT;
 s->auth.owner.control_live=cert_live;
 s->auth.owner.wired_idle=wd;
 s->auth.owner.peer_auth_succeeded=peer;
}
static void reset_fixture(void){
 wired=bt=cap5=cap24=ap=associated=single=bonjour=listener=inner=request=phone_session=1;
 for(int i=1;i<5;i++)media[i]=1;
 ap_calls=0;observed_port=0;native_start_calls=0;
}
static void successful_dual_fallback(void){
 struct cp_cloud_pipeline s;struct cp_step04_session b;struct cp_stock_mfi stock;
 Mu34ApServer real_airplay;
 reset_fixture();mocked_oem(&b,&stock,1);cp_cloud_init(&s);
 assert(cp_cloud_begin(&s,&b,&ops,8,3)==CP_CLOUD_STALE);
 assert(cp_cloud_begin(&s,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_carplay_request(&s)==CP_CLOUD_ORDER);
 assert(cp_cloud_confirm_bootstrap_auth(&s)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&s)==CP_CLOUD_OK&&s.band==CP_RADIO_5);
 assert(ap_calls==1);
 assert(cp_cloud_radio_result(&s,0,0)==CP_CLOUD_RETRY);
 assert(cp_cloud_radio_start(&s)==CP_CLOUD_ORDER);
 assert(cp_cloud_radio_result(&s,0,1)==CP_CLOUD_RETRY);
 assert(cp_cloud_radio_start(&s)==CP_CLOUD_OK&&s.band==CP_RADIO_24);
 assert(cp_cloud_radio_result(&s,1,0)==CP_CLOUD_OK);
 assert(cp_cloud_airplay_ready(&s,"wlan0")==CP_CLOUD_ORDER);
 assert(cp_cloud_carplay_request(&s)==CP_CLOUD_PROOF);
 assert(cp_cloud_prepare_receiver(&s,"wlan0")==CP_CLOUD_PROOF);
 native_airplay(&real_airplay);
 assert(cp_cloud_bind_airplay(&s,&real_airplay)==CP_CLOUD_OK);
 /* Truly call the native binder's setProperty + startServer, once. */
 bonjour=0;
 assert(cp_cloud_prepare_receiver(&s,"wlan0")==CP_CLOUD_RETRY);
 assert(native_start_calls==1);
 bonjour=1;
 assert(cp_cloud_prepare_receiver(&s,"wlan0")==CP_CLOUD_OK);
 assert(native_start_calls==1&&observed_port==5000);
 assert(cp_cloud_carplay_request(&s)==CP_CLOUD_PROOF); /* no real 4301 */
 static const struct cp_session live_session={.airplay_port=5000};
 b.wireless.session=&live_session;b.wireless_bound=1;
 b.wireless.start_sent=1;b.wireless_pending=1;
 assert(cp_cloud_carplay_request(&s)==CP_CLOUD_PROOF); /* ACK pending */
 b.wireless_pending=0;b.link.outbound_pending=0;
 request=0;assert(cp_cloud_carplay_request(&s)==CP_CLOUD_PROOF);
 request=1;assert(cp_cloud_carplay_request(&s)==CP_CLOUD_OK);
 assert(cp_cloud_airplay_ready(&s,"../../etc")==CP_CLOUD_BAD);
 phone_session=0;
 assert(cp_cloud_airplay_ready(&s,"wlan0")==CP_CLOUD_PROOF);
 phone_session=1;
 assert(cp_cloud_airplay_ready(&s,"wlan0")==CP_CLOUD_OK);
 inner=0;assert(cp_cloud_inner_auth(&s)==CP_CLOUD_PROOF);
 inner=1;assert(cp_cloud_inner_auth(&s)==CP_CLOUD_OK);
 inner=0;assert(cp_cloud_media_ready(&s)==CP_CLOUD_PROOF);
 inner=1;
 media[CP_CLOUD_STEERING]=0;
 assert(cp_cloud_media_ready(&s)==CP_CLOUD_PROOF);
 media[CP_CLOUD_STEERING]=1;
 assert(cp_cloud_media_ready(&s)==CP_CLOUD_OK&&cp_cloud_is_active(&s));
 single=0;assert(!cp_cloud_is_active(&s));
 single=1;assert(cp_cloud_is_active(&s));
 inner=0;assert(!cp_cloud_is_active(&s)); /* authenticated inner session expired */
 inner=1;assert(cp_cloud_is_active(&s));
 listener=0;assert(!cp_cloud_is_active(&s));
 listener=1;assert(cp_cloud_is_active(&s));
 phone_session=0;assert(!cp_cloud_is_active(&s));
 phone_session=1;assert(cp_cloud_is_active(&s));
 /* Cached media authorization is invalid after the live AP drops. */
 associated=0;assert(!cp_cloud_is_active(&s));
 associated=1;assert(cp_cloud_is_active(&s));
 cap24=0;assert(!cp_cloud_is_active(&s));
 cap24=1;assert(cp_cloud_is_active(&s));
 assert(cp_cloud_bt_closed(&s,1)==CP_CLOUD_STALE);
 assert(cp_cloud_bt_closed(&s,4)==CP_CLOUD_OK);
 assert(cp_cloud_is_active(&s)); /* stock AirPlay/inner iAP2 owns media */
 assert(cp_radio_revoke(&s.radio,8,0)==CP_RADIO_OK);
 assert(!cp_cloud_is_active(&s)); /* no live radio, no authorized media */
 assert(cp_cloud_wired_preempt(&s)==CP_CLOUD_OK&&!cp_cloud_is_active(&s));
 assert(cp_cloud_end(&s)==CP_CLOUD_OK);
 assert(cp_cloud_begin(&s,&b,&ops,8,4)==CP_CLOUD_ORDER);
}
static void only_24(void){
 struct cp_cloud_pipeline s;struct cp_step04_session b;struct cp_stock_mfi stock;
 reset_fixture();cap5=0;mocked_oem(&b,&stock,1);cp_cloud_init(&s);
 assert(cp_cloud_begin(&s,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&s)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&s)==CP_CLOUD_OK&&s.band==CP_RADIO_24);
 assert(cp_cloud_radio_result(&s,0,1)==CP_CLOUD_PROOF);
 assert(!cp_cloud_is_active(&s));
 assert(cp_cloud_end(&s)==CP_CLOUD_OK);
}
static void no_cert_no_radio(void){
 struct cp_cloud_pipeline s;struct cp_step04_session b;struct cp_stock_mfi stock;
 reset_fixture();mocked_oem(&b,&stock,0);cp_cloud_init(&s);
 assert(cp_cloud_begin(&s,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&s)==CP_CLOUD_PROOF);
 assert(cp_cloud_radio_start(&s)==CP_CLOUD_ORDER);
 cap24=cap5=0;
 assert(cp_cloud_confirm_bootstrap_auth(&s)==CP_CLOUD_PROOF);
 assert(cp_cloud_radio_start(&s)==CP_CLOUD_ORDER);
 assert(cp_cloud_wired_preempt(&s)==CP_CLOUD_OK);
 assert(cp_cloud_end(&s)==CP_CLOUD_OK);
}
static void radio_to_airplay_proof(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 struct cp_oem_assertions proof={0};Mu34ApServer real_ap;
 static const uint8_t mac[6]={1,2,3,4,5,6};
 static const uint16_t sent[]={0xaa01,0xaa03,0x5703,0x4301};
 static const uint16_t recv[]={0xaa00,0xaa02,0xaa05,0x5702,0x4300};
 static const char *const ip[]={"10.0.0.1"};
 static const struct cp_session config={
  .wifi={.ssid="AUDI_REVIEW",.passphrase="review-only",.security=2,.channel=6},
  .ip_addresses=ip,.ip_address_count=1,.airplay_port=5000,
  .public_key="review-only",.source_version="review-only"
 };
 static const struct cp_identification identity={
  .name="Review",.model="MU1005",.manufacturer="Audi",
  .serial="not-real",.firmware="MU1005",.hardware="lab",
  .bluetooth_name="review-bt",.wireless_ssid="AUDI_REVIEW",
  .bluetooth_mac6=mac,.accessory_messages=sent,.phone_messages=recv,
  .accessory_message_count=4,.phone_message_count=5
 };
 reset_fixture();mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 b.link.phase=CP_LK_IDLE;
 assert(cp_cloud_bind_bootstrap(&p,&b,&config,&identity)==CP_CLOUD_OK);
 assert(cp_cloud_bind_bootstrap(&p,&b,&config,&identity)==CP_CLOUD_BAD);
 b.link.phase=CP_LK_NORMAL;
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1);
 assert(!proof.live_hotspot&&!proof.network_airplay_ready);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1);
 assert(proof.verified_mfi&&proof.live_hotspot&&!proof.network_airplay_ready);
 ap=0;
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1&&!proof.live_hotspot);
 ap=1;
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 native_airplay(&real_ap);
 assert(cp_cloud_bind_airplay(&p,&real_ap)==CP_CLOUD_OK);
 assert(cp_cloud_prepare_receiver(&p,"wlan0")==CP_CLOUD_OK);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1);
 assert(proof.live_hotspot&&proof.network_airplay_ready);
 bonjour=0;
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1&&!proof.network_airplay_ready);
 bonjour=1;wired=0;
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==0);
 wired=1;
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==0);
 assert(cp_cloud_end(&p)==CP_CLOUD_OK);
}
static void automatic_native_carplay_start(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 Mu34ApServer real_ap;
 static const uint8_t mac[6]={1,2,3,4,5,6};
 static const uint16_t sent[]={0xaa01,0xaa03,0x5703,0x4301};
 static const uint16_t recv[]={0xaa00,0xaa02,0xaa05,0x5702,0x4300};
 static const char *const ip[]={"10.0.0.1"};
 static const struct cp_session config={
  .wifi={.ssid="AUDI_AUTO",.passphrase="test-review",.security=2,.channel=6},
  .ip_addresses=ip,.ip_address_count=1,.airplay_port=5000,
  .public_key="test-review",.source_version="test-review"
 };
 static const struct cp_identification identity={
  .name="Review",.model="MU1005",.manufacturer="Audi",
  .serial="not-real",.firmware="MU1005",.hardware="lab",
  .bluetooth_name="review-bt",.wireless_ssid="AUDI_AUTO",
  .bluetooth_mac6=mac,.accessory_messages=sent,.phone_messages=recv,
  .accessory_message_count=4,.phone_message_count=5
 };
 reset_fixture();mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 b.link.phase=CP_LK_IDLE;
 assert(cp_cloud_bind_bootstrap(&p,&b,&config,&identity)==CP_CLOUD_OK);
 b.link.phase=CP_LK_NORMAL;
 b.link.epoch=8;b.link.tx_psn=99;b.link.owner.opaque=&b;b.link.owner.live=owner;
 b.link.owner.acked=ack_wireless;
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 native_airplay(&real_ap);
 assert(cp_cloud_bind_airplay(&p,&real_ap)==CP_CLOUD_OK);
 b.identity_accepted=1; /* simulated phone 1D02, no OEM identity fabricated */
 assert(cp_cloud_drive_start(&p,"wlan0",100)==CP_CLOUD_RETRY);
 assert(native_start_calls==1&&b.wireless.start_sent==0);
 /* 4300 arrived: queued authentic iAP2 4301 on the existing link. */
 b.wireless.availability_seen=1;b.wireless.wireless_available=1;
 assert(cp_cloud_drive_start(&p,"wlan0",101)==CP_CLOUD_RETRY);
 assert(b.wireless.start_sent==1&&b.wireless_pending&&b.link.outbound_pending);
 assert(b.link.outbound_seq==100);
 assert(native_start_calls==1);
 /* Simulate the peer's REAL iAP2 ACK wire envelope; not owner assertion. */
 uint8_t ack[9]={0xff,0x5a,0,9,0x40,0,100,0,0};
 ack[8]=sum8(ack,8);
 assert(cp_lk_feed_bytes(&b.link,ack,sizeof ack,102)==CP_LK_OK);
 assert(!b.wireless_pending&&!b.link.outbound_pending);
 phone_session=0;
 assert(cp_cloud_drive_start(&p,"wlan0",103)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_START_REQUESTED);
 assert(cp_cloud_drive_start(&p,"wlan0",104)==CP_CLOUD_RETRY);
 phone_session=1;
 assert(cp_cloud_drive_start(&p,"wlan0",105)==CP_CLOUD_OK);
 assert(p.handoff.phase==CP_HO_AIRPLAY_SESSION);
 assert(cp_cloud_drive_start(&p,"wlan0",104)==CP_CLOUD_OK);
 assert(native_start_calls==1);
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK);
 assert(cp_cloud_end(&p)==CP_CLOUD_OK);
}

/* Symbols exported by test binary are the names resolved by the R69 native
 * binder, but implementations below are isolated host FIXTURES, not OEM. */
struct fake_cf {uint32_t marker;char text[64];};
static int native_owner_ok=1,native_listener=0,native_stop_pending=0,native_media_flag=0;
static int native_inner=1,native_channel[5]={0,1,1,1,1};
static int si_peer_allowed=1,si_peer_checks=0;
static int queue_object,native_stop_calls=0;
static char current_native_iface[32]="eth0";
void *gAirPlayReceiverServer=&airplay_object;
unsigned long CFStringGetTypeID(void){return 0x51ul;}
unsigned long AirPlayReceiverServerGetTypeID(void){return 0x52ul;}
unsigned long CFGetTypeID(const void *p){
 if(p==&airplay_object)return 0x52ul;
 return p&&((const struct fake_cf *)p)->marker==0x46543132u?0x51ul:0;
}
void *CFStringCreateWithCString(const void *allocator,const char *value,unsigned int encoding){
 struct fake_cf *str;
 (void)allocator;
 if(!value||encoding!=0x08000100u||strlen(value)>=64)return NULL;
 str=malloc(sizeof *str);if(!str)return NULL;
 str->marker=0x46543132u;strcpy(str->text,value);return str;
}
unsigned char CFStringGetCString(const void *cf,char *out,long cap,unsigned int encoding){
 const struct fake_cf *str=cf;
 if(!str||CFGetTypeID(cf)!=CFStringGetTypeID()||!out||cap<1||
    encoding!=0x08000100u||strlen(str->text)+1>(size_t)cap)return 0;
 strcpy(out,str->text);return 1;
}
void CFRelease(const void *cf){assert(CFGetTypeID(cf)==CFStringGetTypeID());free((void *)cf);}
void *AirPlayReceiverServerGetDispatchQueue(void *server){
 return server==&airplay_object?&queue_object:NULL;
}
int AirPlayReceiverServerSetProperty(void *server,unsigned int flags,
    const void *key,const void *qualifier,const void *value){
 const struct fake_cf *k=key,*v=value;
 if(server!=&airplay_object||flags||qualifier||
    CFGetTypeID(key)!=CFStringGetTypeID()||
    CFGetTypeID(value)!=CFStringGetTypeID()||
    strcmp(k->text,"interfaceName"))return -1;
 strcpy(current_native_iface,v->text);return 0;
}
int AirPlayReceiverServerControl(void *server,unsigned int flags,
  const void *command,const void *qualifier,const void *value,void **response){
 const struct fake_cf *cmd=command;
 if(server!=&airplay_object||flags!=1||qualifier||value||response||
    CFGetTypeID(command)!=CFStringGetTypeID())return -1;
 if(!strcmp(cmd->text,"startServer")){
  native_start_calls++;native_listener=1;return 0;
 }
 if(!strcmp(cmd->text,"stopServer")){
  native_stop_calls++;native_listener=0;native_stop_pending=1;return 0;
 }
 return -1;
}
static int native_lib(void *ctx,void *h){return ctx==NULL&&h&&native_owner_ok;}
static int native_owner(void *ctx,void *server,const void *manager){
 return ctx==NULL&&server==&airplay_object&&manager==&airplay_owner&&
        native_owner_ok&&wired&&single;
}
static int native_listening(void *ctx,void *server,const void *manager,unsigned port){
 return native_owner(ctx,server,manager)&&native_listener&&port==5000;
}
static int native_stopped(void *ctx,void *server,const void *manager){
 if(!native_owner(ctx,server,manager)||native_listener)return -1;
 if(native_stop_pending){native_stop_pending=0;return 0;}
 return 1;
}
static int native_interface(void *ctx,void *server,const void *manager,const char *iface){
 return native_owner(ctx,server,manager)&&
    !strcmp(iface,current_native_iface);
}
static int rx_admitted(void *ctx,uint32_t generation){
 (void)ctx;return generation==8&&single&&wired;
}
static int rx_si(void *ctx,uint32_t generation,uint64_t epoch,uint32_t session){
 (void)ctx;return generation==8&&epoch==8&&session==1234&&single&&wired;
}
static int rx_peer(void *ctx,int fd,uint32_t gen,uint64_t epoch,uint32_t session){
 struct sockaddr_in peer={0};socklen_t size=sizeof peer;
 (void)ctx;
 si_peer_checks++;
 /* This isolated host adapter checks peer address and a test authorization
  * flag. Production MUST instead bind OS peer credentials to genuine SI. */
 return si_peer_allowed&&fd>=0&&gen==8&&epoch==8&&session==1234&&
  getpeername(fd,(struct sockaddr *)&peer,&size)==0&&
  peer.sin_family==AF_INET&&ntohl(peer.sin_addr.s_addr)==INADDR_LOOPBACK;
}
static int rx_inner(void *ctx,uint32_t gen,uint64_t epoch,uint32_t session){
 (void)ctx;return gen==8&&epoch==8&&session==1234&&native_inner;
}
static int rx_channel(void *ctx,uint32_t gen,uint64_t epoch,uint32_t session,
                      unsigned channel){
 (void)ctx;return gen==8&&epoch==8&&session==1234&&
   channel>=CP_CLOUD_VIDEO&&channel<=CP_CLOUD_STEERING&&native_channel[channel];
}
static int rx_media(void *ctx){(void)ctx;return native_media_flag;}
static int rx_identity(void *ctx,char *s,size_t n){
 (void)ctx;if(n<4)return -1;memcpy(s,"abc",3);return 3;
}
static void native_rx_media_contract(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 MuDioNative native={0};MuCprxService rx={0};
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                         native_stopped,native_interface};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
   .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel};
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 reset_fixture();native_owner_ok=1;native_listener=0;native_stop_pending=0;
 native_media_flag=0;native_inner=1;si_peer_allowed=1;si_peer_checks=0;
 for(int i=1;i<5;i++)native_channel[i]=1;
 strcpy(current_native_iface,"eth0");
 mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(cp_cloud_bind_native_dio(&p,&native)==CP_CLOUD_OK);
 assert(cp_cloud_native_rx_live(&p,0)==0);
 /* No OS peer validator => do not open the privileged v3 listener. */
 MuCprxEvidence missing_peer=evidence;
 missing_peer.si_client_verified=NULL;
 assert(mu_cprx_open_si(&rx,&native,&missing_peer,5000,8,1234)==-1);
 /* Same-process control socket, but dummy handset and media samples. */
 assert(mu_cprx_open_si(&rx,&native,&evidence,5000,8,1234)==0);
 rx.session=1235; /* reject stale/foreign SI session at bind time */
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_PROOF);
 rx.session=1234;
 rx.epoch=9; /* reject wrong epoch even with an authorized generation */
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_PROOF);
 rx.epoch=8;
 rx.evidence.si_inner_iap2_mfi=NULL;
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_PROOF);
 rx.evidence.si_inner_iap2_mfi=rx_inner;
 rx.evidence.si_media_channel=NULL;
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_PROOF);
 rx.evidence.si_media_channel=rx_channel;
 rx.evidence.si_client_verified=NULL;
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_PROOF);
 rx.evidence.si_client_verified=rx_peer;
 /* START must arrive over the real local SI service v3 socket, NOT
  * via an independent call to the original receiver or a forged flag. */
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_OK);
 assert(cp_cloud_native_rx_live(&p,0)==0);
 assert(cp_cloud_prepare_receiver(&p,native.iface)==CP_CLOUD_RETRY);
 assert(native_start_calls==0);
 assert(cp_cloud_drive_native(&p,100)==CP_CLOUD_RETRY);
 /* An unauthorized LOCAL process can know generation and SI session IDs:
  * it still must not reach the native startServer call. */
 int rejected=socket(AF_INET,SOCK_STREAM,0);assert(rejected>=0);
 struct sockaddr_in addr={0};
 addr.sin_family=AF_INET;
 addr.sin_port=htons(MU_CPRX_PORT);
 addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 si_peer_allowed=0;
 assert(connect(rejected,(struct sockaddr *)&addr,sizeof addr)==0);
 unsigned char denied_req[32]={0};
 memcpy(denied_req,"CPRX",4);denied_req[4]=3;denied_req[5]=1;
 denied_req[8]=8;denied_req[12]=0x88;denied_req[13]=0x13;
 denied_req[16]=8;denied_req[24]=0xd2;denied_req[25]=4;
 assert(send(rejected,denied_req,sizeof denied_req,0)==(ssize_t)sizeof denied_req);
 assert(cp_cloud_drive_native(&p,100)==CP_CLOUD_RETRY);
 assert(native_start_calls==0&&!rx.started&&rx.client<0);
 close(rejected);
 si_peer_allowed=1;
 int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 assert(connect(fd,(struct sockaddr *)&addr,sizeof addr)==0);
 /* A previously admitted socket loses authority mid-connection. Its
  * partial request must be dropped BEFORE native side effects. */
 assert(cp_cloud_drive_native(&p,101)==CP_CLOUD_RETRY);
 assert(rx.client>=0);
 si_peer_allowed=0;
 assert(cp_cloud_drive_native(&p,102)==CP_CLOUD_RETRY);
 assert(rx.client<0&&!rx.started&&native_start_calls==0);
 close(fd);
 si_peer_allowed=1;
 fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 assert(connect(fd,(struct sockaddr *)&addr,sizeof addr)==0);
 unsigned char req[32]={0};
 memcpy(req,"CPRX",4);req[4]=3;req[5]=1; /* v3 START */
 req[8]=8;req[12]=0x88;req[13]=0x13; /* gen 8, AirPlay port 5000 */
 req[16]=8;req[24]=0xd2;req[25]=4; /* SI epoch 8/session 1234 */
 assert(send(fd,req,sizeof req,0)==(ssize_t)sizeof req);
 for(unsigned i=0;i<12&&!rx.start_confirmed;i++){
  int got=cp_cloud_drive_native(&p,101+i);
  assert(got==CP_CLOUD_RETRY||got==CP_CLOUD_PROOF);
 }
 assert(rx.started&&rx.start_confirmed);
 assert(si_peer_checks>=2&&native_start_calls==1);
 unsigned char reply[8];
 assert(recv(fd,reply,sizeof reply,0)==(ssize_t)sizeof reply);
 assert(memcmp(reply,"CPRA",4)==0&&reply[5]==0);
 assert(cp_cloud_prepare_receiver(&p,native.iface)==CP_CLOUD_OK);
 assert(native_start_calls==1); /* no competing second START */
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_PROOF);
 assert(cp_cloud_native_rx_live(&p,0)==1);
 assert(cp_cloud_native_rx_live(&p,1)==0);
 p.handoff.phase=CP_HO_AIRPLAY_SESSION;
 strcpy(p.iface,"wlan0");
 /* Owner tick drives inner authentication and stock media in order;
  * it must not hallucinate streaming after merely receiving START. */
 inner=0;
 assert(cp_cloud_drive_native(&p,120)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_AIRPLAY_SESSION);
 inner=1;
 /* Old phone/other inner iAP2 session cannot authorize the current SI. */
 native_inner=0;
 assert(cp_cloud_drive_native(&p,121)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_AIRPLAY_SESSION);
 native_inner=1;
 assert(cp_cloud_drive_native(&p,121)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_INNER_IAP_AUTH);
 native_media_flag=4;
 assert(cp_cloud_drive_native(&p,122)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_INNER_IAP_AUTH);
 native_media_flag=5;
 assert(cp_cloud_drive_native(&p,123)==CP_CLOUD_RETRY);
 native_media_flag=7;
 /* Legacy epoch-only witnesses remain green while the SI session says
  * steering or audio never joined this phone's receiver session. */
 native_channel[CP_CLOUD_STEERING]=0;
 assert(cp_cloud_drive_native(&p,124)==CP_CLOUD_RETRY);
 native_channel[CP_CLOUD_STEERING]=1;
 native_channel[CP_CLOUD_AUDIO]=0;
 assert(cp_cloud_drive_native(&p,124)==CP_CLOUD_RETRY);
 native_channel[CP_CLOUD_AUDIO]=1;
 media[CP_CLOUD_STEERING]=0;
 assert(cp_cloud_drive_native(&p,124)==CP_CLOUD_RETRY);
 media[CP_CLOUD_STEERING]=1;
 assert(cp_cloud_drive_native(&p,125)==CP_CLOUD_OK);
 assert(p.handoff.phase==CP_HO_MEDIA_ACTIVE&&cp_cloud_is_active(&p));
 native_media_flag=0;
 assert(cp_cloud_drive_native(&p,126)==CP_CLOUD_RETRY);
 assert(!cp_cloud_is_active(&p));
 native_media_flag=7;
 assert(cp_cloud_drive_native(&p,127)==CP_CLOUD_OK);
 assert(cp_cloud_is_active(&p));
 native_inner=0;
 assert(!cp_cloud_is_active(&p));
 assert(cp_cloud_drive_native(&p,128)==CP_CLOUD_RETRY);
 native_inner=1;
 native_channel[CP_CLOUD_VIDEO]=0;
 assert(!cp_cloud_is_active(&p));
 native_channel[CP_CLOUD_VIDEO]=1;
 assert(cp_cloud_drive_native(&p,129)==CP_CLOUD_OK);
 assert(cp_cloud_is_active(&p));
 rx.session=1235;assert(!cp_cloud_is_active(&p));
 rx.session=1234;assert(cp_cloud_is_active(&p));
 wired=0;assert(!cp_cloud_native_rx_live(&p,0));
 wired=1;
 close(fd);
 assert(cp_cloud_end(&p)==CP_CLOUD_RETRY);
 assert(native.initialized&&rx.backend_released==0);
 assert(cp_cloud_end(&p)==CP_CLOUD_OK);
 assert(rx.backend_released&&rx.listener==-1);
 assert(!native.initialized&&!strcmp(current_native_iface,"eth0"));
 dlclose(h);
}
static void native_dio_bound_pipeline(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 MuDioNative native={0};
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                         native_stopped,native_interface};
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 reset_fixture();native_owner_ok=1;native_listener=0;native_stop_pending=0;
 native_stop_calls=0;strcpy(current_native_iface,"eth0");
 mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 assert(cp_cloud_drive_native(&p,3)==CP_CLOUD_BAD);
 native_owner_ok=0;
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==-1);
 native_owner_ok=1;
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(native.initialized);
 assert(cp_cloud_bind_native_dio(&p,&native)==CP_CLOUD_OK);
 assert(cp_cloud_bind_native_dio(&p,&native)==CP_CLOUD_PROOF);
 assert(cp_cloud_drive_native(&p,10)==CP_CLOUD_PROOF); /* no wireless */
 /* A bare MuDioNative binding cannot START the receiver without SI CPRX. */
 assert(cp_cloud_prepare_receiver(&p,native.iface)==CP_CLOUD_RETRY);
 assert(native_start_calls==0);
 /* Native low-level binder itself retains the original start/restore ABI;
  * this isolated fixture exercises it without pretending to be the pipeline. */
 assert(mu_dio_start(&native,5000)==MU34_AP_R_LISTENING_VERIFIED);
 assert(native_start_calls==1&&!strcmp(current_native_iface,"wlan0"));
 /* Wired takeover may revoke wireless immediately, but must remain
  * retryable until the original owner completes STOP and restores iface. */
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_RETRY);
 assert(p.stopped&&native.initialized&&native_stop_calls==1);
 assert(p.handoff.wired_active&&p.handoff.phase==CP_HO_TEARDOWN);
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK);
 assert(!native.initialized&&native_stop_calls==1);
 assert(!strcmp(current_native_iface,"eth0"));
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK);
 assert(cp_cloud_end(&p)==CP_CLOUD_OK);
 assert(p.handoff.phase==CP_HO_WIRED);
 dlclose(h);
}
int main(void){successful_dual_fallback();only_24();no_cert_no_radio();radio_to_airplay_proof();automatic_native_carplay_start();native_dio_bound_pipeline();native_rx_media_contract();
 puts("AU57X_R47_CLOUD_04_05_06_07_08_09_OWNER_GATES_PASS");return 0;}
