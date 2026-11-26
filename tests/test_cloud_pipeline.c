/* AU57x owner-bound R04-09 cloud integration fixture: no OEM keys or RF. */
#include "cp_cloud_pipeline.h"
#include "cp_wcp_handoff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
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
#ifdef MU_AP_REFRESH_TEST
#include "../integration/mu1005_si_native/ap_live/ap_refresh.h"
struct ap_refresh_fixture {unsigned calls;int channel,verified,unstable;};
static int refresh_query(void*v,unsigned id,char*out,size_t cap){
 struct ap_refresh_fixture*f=v;unsigned n=f->calls++;
 const char*const vals[]={"BSS status = started\n","SSID = AUDI_REVIEW\n",
  NULL,"AP MAC address = 02:00:00:00:00:01\n","PROTOCOL = WPA2\n","WPA passphrase = fixture-new\n"};
 const char*str=id==2?(f->channel==36?"Channel = 36\nBand    = 5GHz\n":"Channel = 6\nBand    = 2.4GHz\n"):vals[id];
 if(f->unstable&&id==1&&n>=6)str="SSID = DIFFERENT\n";
 if(strlen(str)>=cap)return -1;
 strcpy(out,str);return 0;
}
static int refresh_verify(void*v,uint32_t e,uint32_t g,enum cp_radio_band band,const struct mu_ap_live*s){
 struct ap_refresh_fixture*f=v;
 return f->verified&&e==8&&g==4&&s->channel==f->channel&&
  cp_radio_channel_band(s->channel)==band&&!strcmp(s->ssid,"AUDI_REVIEW")&&!strcmp(s->key,"fixture-new");
}
#endif
static int ap_config_fixture(void *ctx,uint32_t epoch,enum cp_radio_band band,const struct cp_session *cfg){
 return ctx!=NULL&&*(int*)ctx==1&&epoch==8&&
  ((band==CP_RADIO_5&&cfg->wifi.channel==36)||
   (band==CP_RADIO_24&&cfg->wifi.channel==6));
}
static void radio_to_airplay_direction(enum cp_radio_band first){
 enum cp_radio_band next=first==CP_RADIO_5?CP_RADIO_24:CP_RADIO_5;
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 struct cp_oem_assertions proof={0};Mu34ApServer real_ap;
 static const uint8_t mac[6]={1,2,3,4,5,6};
 static const uint16_t sent[]={0xaa01,0xaa03,0x5703,0x4301};
 static const uint16_t recv[]={0xaa00,0xaa02,0xaa05,0x5702,0x4300};
 static const char *const ip[]={"10.0.0.1"};
 struct cp_session config={
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
 config.wifi.channel=first==CP_RADIO_5?36:6;
 reset_fixture();mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 b.link.phase=CP_LK_IDLE;
 assert(cp_cloud_bind_bootstrap(&p,&b,&config,&identity)==CP_CLOUD_OK);
 assert(cp_cloud_bind_bootstrap(&p,&b,&config,&identity)==CP_CLOUD_BAD);
 b.link.phase=CP_LK_NORMAL;
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1);
 assert(!proof.live_hotspot&&!proof.network_airplay_ready);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK&&p.band==first);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1);
 assert(proof.verified_mfi&&proof.live_hotspot&&!proof.network_airplay_ready);
 /* First attempt failed; verify actual disconnect before switching bands. */
 assert(cp_cloud_radio_report_failure(&p,9,4,first)==CP_CLOUD_STALE);
 assert(cp_cloud_radio_report_failure(&p,8,3,first)==CP_CLOUD_STALE);
 assert(cp_cloud_radio_report_failure(&p,8,4,next)==CP_CLOUD_STALE);
 assert(cp_cloud_radio_report_failure(&p,8,4,first)==CP_CLOUD_OK);
 assert(p.radio_failure_pending);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1&&!proof.live_hotspot);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_PROOF);
 assert(cp_cloud_radio_result(&p,0,-1)==CP_CLOUD_BAD);
 assert(cp_cloud_radio_result(&p,0,0)==CP_CLOUD_RETRY);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_ORDER);
 assert(cp_cloud_radio_result(&p,0,1)==CP_CLOUD_RETRY);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK&&p.band==next&&!p.radio_failure_pending);
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1&&!proof.live_hotspot);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_PROOF);
 assert(p.radio.phase==CP_RADIO_CONNECTING&&p.handoff.phase==CP_HO_BOOTSTRAP_AUTH);
 struct cp_session updated=config;updated.wifi.channel=next==CP_RADIO_5?36:6;int verify=0;
 assert(cp_cloud_refresh_ap_config(&p,&updated,ap_config_fixture,&verify)==CP_CLOUD_PROOF);
 verify=1;b.wireless_pending=1;
 assert(cp_cloud_refresh_ap_config(&p,&updated,ap_config_fixture,&verify)==CP_CLOUD_RETRY);
 b.wireless_pending=0;
 assert(cp_cloud_refresh_ap_config(&p,&updated,ap_config_fixture,&verify)==CP_CLOUD_OK);
 assert(b.wireless.session==&updated);
#ifdef MU_AP_REFRESH_TEST
 struct mu_ap_refresh_slot slot={0};
 struct ap_refresh_fixture f={0,first==CP_RADIO_5?36:6,0,0};
 assert(mu_ap_refresh_prepare(&slot,&p,refresh_query,&f)==CP_CLOUD_PROOF);
 f.calls=0;f.channel=updated.wifi.channel;f.unstable=1;
 assert(mu_ap_refresh_prepare(&slot,&p,refresh_query,&f)==CP_CLOUD_PROOF);
 f.calls=0;f.unstable=0;
 assert(mu_ap_refresh_prepare(&slot,&p,refresh_query,&f)==CP_CLOUD_OK&&f.calls==12);
 slot.generation=3;
 assert(mu_ap_refresh_commit(&slot,&p,refresh_verify,&f)==CP_CLOUD_STALE);
 slot.generation=4;
 assert(mu_ap_refresh_commit(&slot,&p,refresh_verify,&f)==CP_CLOUD_PROOF);
 f.verified=1;b.wireless_pending=1;
 assert(mu_ap_refresh_commit(&slot,&p,refresh_verify,&f)==CP_CLOUD_RETRY);
 b.wireless_pending=0;
 assert(mu_ap_refresh_commit(&slot,&p,refresh_verify,&f)==CP_CLOUD_OK);
 assert(b.wireless.session==&slot.session&&slot.session.wifi.channel==updated.wifi.channel);
 assert(slot.session.public_key==updated.public_key&&slot.session.ip_addresses==updated.ip_addresses);
 assert(mu_ap_refresh_clear(&slot,&p)==CP_CLOUD_BAD);
 assert(mu_ap_refresh_prepare(&slot,&p,refresh_query,&f)==CP_CLOUD_BAD);
 puts("PASS AP_READ_TO_AUTHENTICATED_REFRESH_FIXTURE_ONLY_NO_OEM");
#endif
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1&&proof.live_hotspot);
 puts(first==CP_RADIO_5?"PASS 5_TO_24_REFRESH_BEFORE_ASSOCIATION_NO_OEM":"PASS 24_TO_5_REFRESH_BEFORE_ASSOCIATION_NO_OEM");
 ap=0;
 assert(b.wireless_proof(b.wireless_ctx,8,4,&proof)==1&&!proof.live_hotspot);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_PROOF);
 assert(p.radio.phase==CP_RADIO_CONNECTING);
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
#ifdef MU_AP_REFRESH_TEST
 /* Owner has ended this fixture; explicitly release the borrowed pointer. */
 b.wireless.session=NULL;
 assert(mu_ap_refresh_clear(&slot,&p)==CP_CLOUD_OK);
 for(size_t i=0;i<sizeof slot;i++)assert(((unsigned char*)&slot)[i]==0);
#endif
}
static void radio_to_airplay_proof(void){
 radio_to_airplay_direction(CP_RADIO_5);
 radio_to_airplay_direction(CP_RADIO_24);
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
 struct sockaddr_un peer={0};socklen_t size=sizeof peer;
 (void)ctx;
 si_peer_checks++;
 /* Host-only SI admission fixture; native kernel credentials are checked
  * separately by the server before this callback is called. */
 return si_peer_allowed&&fd>=0&&gen==8&&epoch==8&&session==1234&&
  getpeername(fd,(struct sockaddr *)&peer,&size)==0&&
  peer.sun_family==AF_UNIX;
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
 assert(cp_cloud_enable_native_client(&p)==CP_CLOUD_PROOF);
 assert(cp_cloud_native_rx_live(&p,0)==0);
 /* v3 TCP entrypoint must stay fail-closed. Only a preexisting 0700
  * owner-private directory may host a credential-checked AF_UNIX listener. */
 assert(mu_cprx_open_si(&rx,&native,&evidence,5000,8,1234)==-1);
 char sockdir[]="/tmp/au57x-r76-si-XXXXXX";
 assert(mkdtemp(sockdir));
 assert(chmod(sockdir,0777)==0);
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                              sockdir,geteuid(),getegid())==-1);
 assert(chmod(sockdir,0700)==0);
 MuCprxEvidence missing_peer=evidence;
 missing_peer.si_client_verified=NULL;
 assert(mu_cprx_open_si_unix(&rx,&native,&missing_peer,5000,8,1234,
                              sockdir,geteuid(),getegid())==-1);
 /* Valid SI lease and actual kernel AF_UNIX peer credentials. */
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                             sockdir,geteuid(),getegid())==0);
 assert(rx.unix_listener&&rx.unix_path[0]);
 struct stat sockstat;
 assert(lstat(rx.unix_path,&sockstat)==0&&S_ISSOCK(sockstat.st_mode));
 assert((sockstat.st_mode&0777)==0600);
 char bound_path[MU_CPRX_UNIX_PATH_MAX];
 strcpy(bound_path,rx.unix_path);
 /* Never remove/replace an existing receiver socket. */
 MuCprxService other={0};
 assert(mu_cprx_open_si_unix(&other,&native,&evidence,5000,8,1234,
                             sockdir,geteuid(),getegid())==-1);
 /* A second open with a DIFFERENT path must not overwrite the live service.
  * A pathname-only collision check would miss this ownership bug. */
 char elsewhere[]="/tmp/au57x-r77-other-XXXXXX";
 assert(mkdtemp(elsewhere));
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                             elsewhere,geteuid(),getegid())==-1);
 assert(strcmp(rx.unix_path,bound_path)==0&&rx.listener>=0);
 assert(rmdir(elsewhere)==0);
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
 int rejected=socket(AF_UNIX,SOCK_STREAM,0);assert(rejected>=0);
 struct sockaddr_un addr={0};
 addr.sun_family=AF_UNIX;
 strcpy(addr.sun_path,rx.unix_path);
 /* A matching AF_INET connection to the obsolete port cannot reach v3. */
 int tcp=socket(AF_INET,SOCK_STREAM,0);assert(tcp>=0);
 struct sockaddr_in old={0};old.sin_family=AF_INET;
 old.sin_addr.s_addr=htonl(INADDR_LOOPBACK);old.sin_port=htons(MU_CPRX_PORT);
 assert(connect(tcp,(struct sockaddr*)&old,sizeof old)==-1);
 close(tcp);
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
 /* Kernel UID mismatch must be rejected independent of SI wire/session. */
 rx.peer_uid=geteuid()==0?1:0;
 int wrong=socket(AF_UNIX,SOCK_STREAM,0);assert(wrong>=0);
 assert(connect(wrong,(struct sockaddr*)&addr,sizeof addr)==0);
 assert(cp_cloud_drive_native(&p,100)==CP_CLOUD_RETRY);
 assert(rx.client<0&&native_start_calls==0);
 close(wrong);rx.peer_uid=geteuid();
 int fd=socket(AF_UNIX,SOCK_STREAM,0);assert(fd>=0);
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
 /* Fail closed before opening any client socket on stale SI generations,
  * altered kernel credentials or a switched socket inode. */
 MuCprxClient bad={0};mu_cprx_client_init(&bad);
 rx.session=1235;
 assert(mu_cprx_client_begin(&bad,&rx,103)==-1&&bad.phase==0);
 rx.session=1234;
 rx.peer_uid=(geteuid()==0?1:0);
 assert(mu_cprx_client_begin(&bad,&rx,103)==-1&&bad.phase==0);
 rx.peer_uid=geteuid();
 rx.unix_ino++;
 assert(mu_cprx_client_begin(&bad,&rx,103)==-1&&bad.phase==0);
 rx.unix_ino--;
 assert(!rx.started&&native_start_calls==0);
 /* R78: original SI owner opts in; the pipeline MUST construct and send
  * the actual CPRX v3 START, no manually composed success packet. */
 assert(cp_cloud_enable_native_client(&p)==CP_CLOUD_OK);
 assert(cp_cloud_enable_native_client(&p)==CP_CLOUD_PROOF);
 for(unsigned i=0;i<25&&!rx.start_confirmed;i++){
  int got=cp_cloud_drive_native(&p,103+i);
  assert(got==CP_CLOUD_RETRY||got==CP_CLOUD_PROOF);
 }
 assert(rx.started&&rx.start_confirmed);
 assert(p.rx_client_enabled&&p.rx_client.phase==4&&p.rx_client.fd==-1);
 MuCprxClient duplicate={0};mu_cprx_client_init(&duplicate);
 assert(mu_cprx_client_begin(&duplicate,&rx,114)==-1);
 assert(duplicate.phase==0);
 assert(si_peer_checks>=2&&native_start_calls==1);
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
 /* Even a previously acknowledged START must fail if SI lease changes. */
 MuCprxClient stale=p.rx_client;
 assert(mu_cprx_client_tick(&stale,130)==MU_CPRX_CLIENT_ERROR);
 rx.session=1234;assert(cp_cloud_is_active(&p));
 wired=0;assert(!cp_cloud_native_rx_live(&p,0));
 /* R80: an actual wired takeover must be seen by the existing owner tick,
  * not depend on an unrelated UI event manually calling wired_preempt(). */
 assert(cp_cloud_drive_native(&p,130)==CP_CLOUD_RETRY);
 assert(p.stopped&&p.handoff.wired_active);
 assert(native.initialized&&rx.backend_released==0);
 assert(rx.listener==-1&&rx.unix_listener);
 wired=1;
 /* STOP is pending: keep the *same* AF_UNIX path reserved so a new
  * controller cannot race the original AirPlay singleton restoration. */
 assert(lstat(bound_path,&sockstat)==0&&S_ISSOCK(sockstat.st_mode));
 assert(mu_cprx_open_si_unix(&other,&native,&evidence,5000,8,1234,
                             sockdir,geteuid(),getegid())==-1);
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                             elsewhere,geteuid(),getegid())==-1);
 assert(cp_cloud_drive_native(&p,131)==CP_CLOUD_RETRY);
 assert(cp_cloud_drive_native(&p,132)==CP_CLOUD_OK);
 assert(rx.backend_released&&rx.listener==-1);
 assert(!p.rx_client_enabled&&!p.rx_client.phase&&p.rx_client.fd==-1);
 assert(!native.initialized&&!strcmp(current_native_iface,"eth0"));
 assert(lstat(bound_path,&sockstat)==-1&&errno==ENOENT);
 assert(rmdir(sockdir)==0);
 dlclose(h);
}
/* R79: exercise actual CPRX STOP protocol in addition to direct OEM
 * rollback. BUSY must not stop live phone media. Idle STOP must await the
 * original backend's multi-tick restoration, not only write a request. */
static void native_graceful_stop_protocol(void){
 MuDioNative native={0};MuCprxService rx={0};
 MuCprxClient start,stopper;
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                         native_stopped,native_interface};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
   .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel};
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 char dir[]="/tmp/au57x-r79-stop-XXXXXX";
 reset_fixture();native_owner_ok=1;native_listener=0;
 native_stop_pending=0;native_stop_calls=0;native_media_flag=0;
 si_peer_allowed=1;strcpy(current_native_iface,"eth0");
 assert(mkdtemp(dir));
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                              dir,geteuid(),getegid())==0);
 char unix_path[MU_CPRX_UNIX_PATH_MAX];strcpy(unix_path,rx.unix_path);
 mu_cprx_client_init(&start);mu_cprx_client_init(&stopper);
 assert(mu_cprx_client_begin_stop(&stopper,&rx,200)==-1);
 assert(mu_cprx_client_begin(&start,&rx,200)==0);
 int result=MU_CPRX_CLIENT_PENDING;
 for(unsigned i=0;i<24&&result==MU_CPRX_CLIENT_PENDING;i++){
  result=mu_cprx_client_tick(&start,200+i);
  assert(result==MU_CPRX_CLIENT_PENDING||result==MU_CPRX_CLIENT_CONFIRMED);
  assert(mu_cprx_tick(&rx,200+i)==0);
  if(result==MU_CPRX_CLIENT_PENDING)
   result=mu_cprx_client_tick(&start,200+i);
 }
 assert(result==MU_CPRX_CLIENT_CONFIRMED);
 assert(rx.started&&rx.start_confirmed&&native_start_calls==1);
 native_media_flag=7;
 assert(mu_cprx_client_begin_stop(&stopper,&rx,300)==0);
 result=MU_CPRX_CLIENT_PENDING;
 for(unsigned i=0;i<24&&result==MU_CPRX_CLIENT_PENDING;i++){
  result=mu_cprx_client_tick(&stopper,300+i);
  assert(result==MU_CPRX_CLIENT_PENDING||result==MU_CPRX_CLIENT_BUSY);
  assert(mu_cprx_tick(&rx,300+i)==0);
  if(result==MU_CPRX_CLIENT_PENDING)
   result=mu_cprx_client_tick(&stopper,300+i);
 }
 assert(result==MU_CPRX_CLIENT_BUSY);
 assert(rx.started&&rx.start_confirmed&&native_stop_calls==0);
 assert(mu_cprx_client_tick(&stopper,400)==MU_CPRX_CLIENT_BUSY);
 mu_cprx_client_close(&stopper);
 /* Correct media-off confirmation enables the SAME leased receiver to STOP.
  * R77's pathname remains reserved until native backend restores. */
 native_media_flag=0;
 assert(mu_cprx_client_begin_stop(&stopper,&rx,500)==0);
 result=MU_CPRX_CLIENT_PENDING;
 int saw_pending_restore=0;
 for(unsigned i=0;i<32&&result==MU_CPRX_CLIENT_PENDING;i++){
  result=mu_cprx_client_tick(&stopper,500+i);
  assert(result==MU_CPRX_CLIENT_PENDING||result==MU_CPRX_CLIENT_CONFIRMED);
  int srv=mu_cprx_tick(&rx,500+i);
  assert(srv==0||srv==1);
  if(native_stop_calls==1&&!rx.backend_released){
   struct stat st;
   saw_pending_restore=1;
   assert(lstat(unix_path,&st)==0&&S_ISSOCK(st.st_mode));
  }
  if(result==MU_CPRX_CLIENT_PENDING)
   result=mu_cprx_client_tick(&stopper,500+i);
 }
 assert(result==MU_CPRX_CLIENT_CONFIRMED);
 assert(saw_pending_restore);
 assert(rx.backend_released&&native_stop_calls==1);
 assert(!native.initialized&&!strcmp(current_native_iface,"eth0"));
 struct stat st;
 assert(lstat(unix_path,&st)==-1&&errno==ENOENT);
 assert(mu_cprx_client_tick(&start,601)==MU_CPRX_CLIENT_ERROR);
 assert(mu_cprx_client_tick(&stopper,601)==MU_CPRX_CLIENT_CONFIRMED);
 mu_cprx_client_close(&start);mu_cprx_client_close(&stopper);
 assert(rmdir(dir)==0);dlclose(h);
}
/* R79 end-to-end pipeline: verified native START -> media-aware STOP ->
 * confirmed native restoration and original session teardown. */
static void native_graceful_pipeline_end(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 MuDioNative native={0};MuCprxService rx={0};
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                         native_stopped,native_interface};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
   .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel};
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 char dir[]="/tmp/au57x-r79-pipeline-XXXXXX";
 reset_fixture();native_owner_ok=1;native_listener=0;
 native_stop_pending=0;native_stop_calls=0;native_media_flag=0;
 si_peer_allowed=1;strcpy(current_native_iface,"eth0");
 mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(cp_cloud_bind_native_dio(&p,&native)==CP_CLOUD_OK);
 assert(mkdtemp(dir));
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                              dir,geteuid(),getegid())==0);
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_OK);
 assert(cp_cloud_enable_native_client(&p)==CP_CLOUD_OK);
 for(unsigned i=0;i<24&&!rx.start_confirmed;i++){
  int rc=cp_cloud_drive_native(&p,200+i);
  assert(rc==CP_CLOUD_RETRY||rc==CP_CLOUD_PROOF);
 }
 assert(rx.started&&rx.start_confirmed&&p.rx_client.phase==4);
 native_media_flag=7;
 assert(cp_cloud_graceful_native_end(&p,300)==CP_CLOUD_RETRY);
 assert(!p.graceful_stop_started&&native_stop_calls==0&&rx.started);
 native_media_flag=0;
 int r=CP_CLOUD_RETRY;
 for(unsigned i=0;i<32&&r==CP_CLOUD_RETRY;i++){
  r=cp_cloud_graceful_native_end(&p,301+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_OK);
 }
 assert(r==CP_CLOUD_OK);
 assert(p.stopped&&!p.native_dio&&!p.rx_service&&!p.rx_client.phase);
 assert(rx.backend_released&&native_stop_calls==1);
 assert(!native.initialized&&!strcmp(current_native_iface,"eth0"));
 assert(rmdir(dir)==0);
 dlclose(h);
}
/* R80: independent SI session loss after a real START must force an
 * automatic native rollback across owner ticks. The first recovery attempt
 * cannot restore while OEM owner proof is absent; the pathname stays leased
 * and must only be freed after restoring original DIO and interface. */
static void native_si_owner_loss_recovery(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 MuDioNative native={0};MuCprxService rx={0};
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                         native_stopped,native_interface};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
   .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel};
 char dir[]="/tmp/au57x-r80-loss-XXXXXX";
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 reset_fixture();native_owner_ok=1;native_listener=0;native_stop_calls=0;
 native_stop_pending=0;native_media_flag=0;si_peer_allowed=1;
 strcpy(current_native_iface,"eth0");
 mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(cp_cloud_bind_native_dio(&p,&native)==CP_CLOUD_OK);
 assert(mkdtemp(dir));
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                              dir,geteuid(),getegid())==0);
 char path[MU_CPRX_UNIX_PATH_MAX];strcpy(path,rx.unix_path);
 assert(cp_cloud_bind_rx_service(&p,&rx)==CP_CLOUD_OK);
 assert(cp_cloud_enable_native_client(&p)==CP_CLOUD_OK);
 for(unsigned i=0;i<24&&!rx.start_confirmed;i++){
  int r=cp_cloud_drive_native(&p,200+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
 }
 assert(rx.start_confirmed&&native_start_calls==1);
 /* Simulate real SI owner's disappearance, not merely a transient video
  * pause: old socket ACK is no longer a valid session credential. */
 single=0;
 assert(cp_cloud_drive_native(&p,300)==CP_CLOUD_RETRY);
 assert(p.stopped&&p.native_cleanup_pending);
 assert(rx.listener==-1&&!rx.backend_released);
 struct stat st;assert(lstat(path,&st)==0&&S_ISSOCK(st.st_mode));
 single=1;
 assert(cp_cloud_drive_native(&p,301)==CP_CLOUD_RETRY);
 assert(lstat(path,&st)==0&&S_ISSOCK(st.st_mode));
 assert(cp_cloud_drive_native(&p,302)==CP_CLOUD_STOPPED);
 assert(!p.native_cleanup_pending&&!p.native_dio&&!p.rx_service);
 assert(rx.backend_released&&native_stop_calls==1);
 assert(!native.initialized&&!strcmp(current_native_iface,"eth0"));
 assert(lstat(path,&st)==-1&&errno==ENOENT);
 assert(rmdir(dir)==0);
 dlclose(h);
}
/* R80: a disconnected CPRX client must not abandon an in-flight OEM START.
 * The socket is a real AF_UNIX connection, but OEM operations are stubs. */
struct r80_pending_backend {int starts,restores;};
static int r80_start(void *opaque,unsigned port){
 struct r80_pending_backend *p=opaque;
 assert(port==5000);p->starts++;return 1; /* asynchronous pending */
}
static int r80_poll(void *opaque){(void)opaque;return 1;}
static int r80_restore(void *opaque){
 struct r80_pending_backend *p=opaque;
 return ++p->restores==1?1:0; /* pending, then complete */
}
static void native_abandoned_mutation_rollback(void){
 MuCprxService rx={0};struct r80_pending_backend status={0};
 MuReceiverBackend b={&status,8,r80_start,r80_poll,r80_restore};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer};
 char dir[]="/tmp/au57x-r80-abort-XXXXXX";
 reset_fixture();si_peer_allowed=1;assert(mkdtemp(dir));
 assert(mu_cprx_open_backend_si_unix(&rx,&b,&evidence,5000,8,1234,
                                      dir,geteuid(),getegid())==0);
 char path[MU_CPRX_UNIX_PATH_MAX];strcpy(path,rx.unix_path);
 int fd=socket(AF_UNIX,SOCK_STREAM,0);assert(fd>=0);
 struct sockaddr_un addr={0};addr.sun_family=AF_UNIX;
 strcpy(addr.sun_path,path);
 assert(connect(fd,(struct sockaddr*)&addr,sizeof addr)==0);
 unsigned char req[32]={0};
 memcpy(req,"CPRX",4);req[4]=3;req[5]=1;
 req[8]=8;req[12]=0x88;req[13]=0x13;
 req[16]=8;req[24]=0xd2;req[25]=4;
 assert(send(fd,req,sizeof req,0)==(ssize_t)sizeof req);
 for(int i=0;i<6&&status.starts==0;i++)
  assert(mu_cprx_tick(&rx,100+i)==0);
 assert(status.starts==1&&rx.mutating&&rx.started&&!rx.start_confirmed);
 close(fd); /* pending original AirPlay operation becomes orphaned */
 assert(mu_cprx_tick(&rx,110)==-1);
 assert(rx.terminal&&rx.listener==-1&&!rx.backend_released);
 assert(status.restores==1);
 struct stat st;assert(lstat(path,&st)==0&&S_ISSOCK(st.st_mode));
 MuCprxService duplicate={0};
 assert(mu_cprx_open_backend_si_unix(&duplicate,&b,&evidence,5000,8,1234,
                                      dir,geteuid(),getegid())==-1);
 assert(mu_cprx_shutdown(&rx)==1&&rx.backend_released);
 assert(status.restores==2);
 assert(lstat(path,&st)==-1&&errno==ENOENT);
 assert(rmdir(dir)==0);
}
/* R81 architecture test: real SI client and DIO receiver live in different
 * Unix processes. No shared MuCprxService address or OEM global objects. */
struct r81_backend { unsigned starts,stops; };
static int r81_start(void *ctx,unsigned port){
 struct r81_backend *b=ctx;assert(port==5000);b->starts++;return 2;
}
static int r81_poll(void *ctx){(void)ctx;return 2;}
static int r81_restore(void *ctx){
 struct r81_backend *b=ctx;b->stops++;return 0;
}
static int r81_client_si_allowed=1,r81_client_dio_allowed=1;
static int r81_si_owner(void *ctx,uint32_t gen,uint64_t epoch,uint32_t sess){
 (void)ctx;return r81_client_si_allowed&&gen==8&&epoch==8&&sess==1234;
}
static int r81_dio_server(void *ctx,int fd,uint32_t gen,uint64_t epoch,uint32_t sess){
 struct sockaddr_un sa={0};socklen_t len=sizeof sa;
 (void)ctx;
 return r81_client_dio_allowed&&r81_si_owner(ctx,gen,epoch,sess)&&fd>=0&&
  getpeername(fd,(struct sockaddr*)&sa,&len)==0&&sa.sun_family==AF_UNIX;
}
/* Test-only synchronization of parent DIO observations and child SI calls.
 * The two processes do not share C objects or synthetic status globals. */
static void r83_set_native(int to_server,int from_server,char value){
 char reply=0;
 assert(write(to_server,&value,1)==1);
 assert(read(from_server,&reply,1)==1&&reply==value);
}
static void native_remote_process_boundary(void){
 struct r81_backend b={0};MuCprxService rx={0};
 MuReceiverBackend backend={&b,8,r81_start,r81_poll,r81_restore};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
   .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel};
 char dir[]="/tmp/au57x-r81-separate-XXXXXX";
 MuCprxRemoteEndpoint endpoint={0};
 reset_fixture();si_peer_allowed=1;native_inner=1;
 for(int i=1;i<=4;i++)native_channel[i]=1;
 /* DIO reports state 4: associated/pending, not video/audio ready. */
 native_media_flag=4;
 assert(mkdtemp(dir));
 assert(mu_cprx_open_backend_si_unix(&rx,&backend,&evidence,5000,8,1234,
                          dir,geteuid(),getegid())==0);
 strcpy(endpoint.socket_path,rx.unix_path);
 endpoint.generation=8;endpoint.epoch=8;endpoint.session=1234;
 endpoint.airplay_port=5000;endpoint.server_uid=geteuid();
 endpoint.server_gid=getegid();
 endpoint.si_owner_verified=r81_si_owner;
 endpoint.dio_server_verified=r81_dio_server;
 int changes[2],acks[2];
 assert(pipe(changes)==0&&pipe(acks)==0);
 pid_t pid=fork();assert(pid>=0);
 if(pid==0){
  close(changes[0]);close(acks[1]);
  /* Separate address space: this child NEVER accesses rx or backend. */
  MuCprxClient client;mu_cprx_client_init(&client);
  MuCprxRemoteEndpoint wrong=endpoint;
  wrong.si_owner_verified=NULL;
  assert(mu_cprx_client_begin_remote(&client,&wrong,1,100)==-1);
  wrong=endpoint;wrong.dio_server_verified=NULL;
  assert(mu_cprx_client_begin_remote(&client,&wrong,1,100)==-1);
  wrong=endpoint;wrong.session=1235;
  assert(mu_cprx_client_begin_remote(&client,&wrong,1,100)==-1);
  wrong=endpoint;wrong.server_uid=(geteuid()==0?1:0);
  assert(mu_cprx_client_begin_remote(&client,&wrong,1,100)==-1);
  /* The original SI owner and DIO identity may vanish after connect but
   * BEFORE any START bytes are sent; deny even this transient window. */
  assert(mu_cprx_client_begin_remote(&client,&endpoint,1,100)==0);
  r81_client_dio_allowed=0;
  assert(mu_cprx_client_tick(&client,100)==MU_CPRX_CLIENT_ERROR);
  mu_cprx_client_close(&client);r81_client_dio_allowed=1;
  assert(mu_cprx_client_begin_remote(&client,&endpoint,1,100)==0);
  r81_client_si_allowed=0;
  assert(mu_cprx_client_tick(&client,100)==MU_CPRX_CLIENT_ERROR);
  mu_cprx_client_close(&client);r81_client_si_allowed=1;
  /* Before START, a read-only status query cannot claim a DIO listener. */
  assert(mu_cprx_client_begin_remote(&client,&endpoint,MU_CPRX_OP_STATUS,100)==0);
  int rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,100+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_BUSY&&client.observed_media==-1);
  mu_cprx_client_close(&client);
  assert(mu_cprx_client_begin_remote(&client,&endpoint,1,100)==0);
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,100+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_CONFIRMED&&client.remote_mode);
  assert(client.service==NULL&&client.fd==-1);
  mu_cprx_client_close(&client);
  /* Read an actual 12-byte DIO response in the OTHER PROCESS. The fact
   * that the receiver started is not equivalent to AirPlay media=7. */
  assert(mu_cprx_client_begin_remote(&client,&endpoint,MU_CPRX_OP_STATUS,620)==0);
  assert(client.observed_media==-1&&client.reply_need==8);
  /* Status success extends 8-byte CPRA header to exactly 12 bytes. */
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,620+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_CONFIRMED&&client.observed_media==4&&client.reply_need==12);
  assert(client.reply[8]==4&&client.reply[9]==0&&client.reply[10]==0&&
         client.reply[11]==0);
  /* R83: a missing DIO same-session verifier must FAIL CLOSED.
   * A valid global media flag and valid wire IDs are insufficient. */
  mu_cprx_client_close(&client);
  r83_set_native(changes[1],acks[0],'n');
  assert(mu_cprx_client_begin_remote(&client,&endpoint,
            MU_CPRX_OP_SESSION_STATUS,625)==0);
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,625+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_ERROR&&client.observed_media==-1&&
         client.observed_inner==-1&&client.observed_channels==0);
  mu_cprx_client_close(&client);
  /* Native DIO media=7 alone is not complete channel delivery: revoke the
   * real steering callback and observe mask 0b0111 for the SAME SI session. */
  r83_set_native(changes[1],acks[0],'7');
  assert(mu_cprx_client_begin_remote(&client,&endpoint,
            MU_CPRX_OP_SESSION_STATUS,630)==0);
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,630+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_CONFIRMED&&client.reply_need==28&&
         client.observed_media==7&&client.observed_inner==1&&
         client.observed_channels==7&&client.reply[11]==0);
  mu_cprx_client_close(&client);
  r83_set_native(changes[1],acks[0],'a');
  assert(mu_cprx_client_begin_remote(&client,&endpoint,
            MU_CPRX_OP_SESSION_STATUS,640)==0);
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,640+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_CONFIRMED&&client.observed_media==7&&
         client.observed_inner==1&&client.observed_channels==15);
  /* Explicit SI owner revocation invalidates the previously read snapshot. */
  r81_client_si_allowed=0;
  assert(mu_cprx_client_tick(&client,645)==MU_CPRX_CLIENT_ERROR);
  assert(client.observed_media==-1&&client.observed_inner==-1&&
         client.observed_channels==0);
  mu_cprx_client_close(&client);r81_client_si_allowed=1;
  r83_set_native(changes[1],acks[0],'0');
  assert(mu_cprx_client_begin_remote(&client,&endpoint,
            MU_CPRX_OP_SESSION_STATUS,646)==0);
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,646+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_CONFIRMED&&client.observed_media==4&&
         client.observed_inner==1&&client.observed_channels==0);
  /* A completed status sample isn't permanent authorization. */
  r81_client_si_allowed=0;
  assert(mu_cprx_client_tick(&client,630)==MU_CPRX_CLIENT_ERROR);
  mu_cprx_client_close(&client);r81_client_si_allowed=1;
  assert(mu_cprx_client_begin_remote(&client,&endpoint,2,650)==0);
  rc=MU_CPRX_CLIENT_PENDING;
  for(unsigned i=0;i<500&&rc==MU_CPRX_CLIENT_PENDING;i++){
   rc=mu_cprx_client_tick(&client,650+i);
   if(rc==MU_CPRX_CLIENT_PENDING)usleep(1000);
  }
  assert(rc==MU_CPRX_CLIENT_CONFIRMED);
  mu_cprx_client_close(&client);
  close(changes[1]);close(acks[0]);
  _exit(0);
 }
 close(changes[1]);close(acks[0]);
 int fl=fcntl(changes[0],F_GETFL);
 assert(fl>=0&&fcntl(changes[0],F_SETFL,fl|O_NONBLOCK)==0);
 int server_end=0;
 for(unsigned i=0;i<1500&&!server_end;i++){
  char command;
  while(read(changes[0],&command,1)==1){
   if(command=='n')rx.evidence.si_inner_iap2_mfi=NULL;
   if(command=='7'){
    rx.evidence.si_inner_iap2_mfi=rx_inner;
    native_media_flag=7;native_channel[4]=0;
   }
   if(command=='a')native_channel[4]=1;
   if(command=='0')native_media_flag=4;
   assert(write(acks[1],&command,1)==1);
  }
  int rc=mu_cprx_tick(&rx,100+i);
  assert(rc==0||rc==1);
  server_end=rx.backend_released;
  if(!server_end)usleep(500);
 }
 assert(server_end&&b.starts==1&&b.stops==1);
 assert(native_media_flag==4); /* read-only DIO media query must not mutate */
 close(changes[0]);close(acks[1]);
 int childstatus=0;assert(waitpid(pid,&childstatus,0)==pid);
 assert(WIFEXITED(childstatus)&&WEXITSTATUS(childstatus)==0);
 struct stat st;assert(lstat(endpoint.socket_path,&st)==-1&&errno==ENOENT);
 assert(rmdir(dir)==0);
}
/* R83: hostile/corrupt remote replies cannot impersonate the current SI
 * session or claim four synchronized channels from a global media flag.
 * The parser must reject invalid generation/epoch/session, partial proofs,
 * reserved bits, and unsupported media without leaving cached grants. */
static void native_r83_corrupt_session_snapshot(void){
 MuCprxRemoteEndpoint e={0};
 e.generation=8;e.epoch=8;e.session=1234;
 e.si_owner_verified=r81_si_owner;
 e.dio_server_verified=r81_dio_server;
 r81_client_si_allowed=r81_client_dio_allowed=1;
 for(int variant=0;variant<9;variant++){
  int fd[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
  MuCprxClient c;mu_cprx_client_init(&c);
  c.remote=e;c.remote_mode=1;c.operation=MU_CPRX_OP_SESSION_STATUS;
  c.generation=8;c.epoch=8;c.session=1234;
  c.fd=fd[0];c.phase=3;c.reply_need=8;c.deadline=1000;
  unsigned char reply[28]={0};
  memcpy(reply,"CPRA",4);reply[4]=2;
  reply[8]=7;reply[9]=1;reply[10]=15;
  reply[12]=8;reply[16]=8;reply[24]=0xd2;reply[25]=4;
  switch(variant){
   case 0:reply[12]=9;break; /* wrong generation */
   case 1:reply[16]=9;break; /* stale epoch */
   case 2:reply[24]=0xd3;break; /* different SI session */
   case 3:reply[8]=4;break; /* incomplete native media */
   case 4:reply[9]=0;break; /* missing inner MFi */
   case 5:reply[10]=0x1f;break; /* forbidden channel bits */
   case 6:reply[11]=1;break; /* malformed reserved */
   case 7:reply[8]=8;break; /* undocumented native state */
   default:break; /* exact same-session, full snapshot parses */
  }
  assert(send(fd[1],reply,sizeof reply,0)==(ssize_t)sizeof reply);
  int r=mu_cprx_client_tick(&c,100);
  assert(r==MU_CPRX_CLIENT_PENDING&&c.reply_need==28);
  r=mu_cprx_client_tick(&c,101);
  if(variant!=8){
   assert(r==MU_CPRX_CLIENT_ERROR&&c.observed_media==-1&&
          c.observed_inner==-1&&c.observed_channels==0);
  }else{
   assert(r==MU_CPRX_CLIENT_CONFIRMED&&c.observed_media==7&&
          c.observed_inner==1&&c.observed_channels==15);
  }
  mu_cprx_client_close(&c);close(fd[1]);
 }
}
/* R84: SI-side cloud pipeline coordinates a DIO endpoint that has NO
 * same-process MuDioNative* or MuCprxService* attached to the pipeline.
 * The server's OEM operations are host stubs in a separate owner tick;
 * R81 still exercises the same wire protocol in forked real processes. */
static void native_remote_owner_cloud_pipeline(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 MuDioNative native={0};MuCprxService rx={0};
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                        native_stopped,native_interface};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
   .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
   .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel};
 static const uint8_t mac[6]={1,2,3,4,5,6};
 static const uint16_t sent[]={0xaa01,0xaa03,0x5703,0x4301};
 static const uint16_t received[]={0xaa00,0xaa02,0xaa05,0x5702,0x4300};
 static const char *const ip[]={"10.0.0.1"};
 static const struct cp_session config={
  .wifi={.ssid="R84_FIXTURE",.passphrase="not-a-real-key",.security=2,.channel=6},
  .ip_addresses=ip,.ip_address_count=1,.airplay_port=5000,
  .public_key="only-a-host-test",.source_version="only-a-host-test"
 };
 static const struct cp_identification identity={
  .name="R84",.model="MU1005",.manufacturer="Audi",.serial="not-real",
  .firmware="MU1005",.hardware="offline",.bluetooth_name="offline-bt",
  .wireless_ssid="R84_FIXTURE",.bluetooth_mac6=mac,
  .accessory_messages=sent,.phone_messages=received,
  .accessory_message_count=4,.phone_message_count=5
 };
 MuCprxRemoteEndpoint e={0};
 char dir[]="/tmp/au57x-r84-cloud-XXXXXX";
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 reset_fixture();native_owner_ok=1;native_listener=0;
 native_stop_pending=0;native_stop_calls=0;
 native_media_flag=4;native_inner=1;si_peer_allowed=1;
 for(int i=1;i<=4;i++)native_channel[i]=1;
 strcpy(current_native_iface,"eth0");
 mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 b.link.phase=CP_LK_IDLE;
 assert(cp_cloud_bind_bootstrap(&p,&b,&config,&identity)==CP_CLOUD_OK);
 b.link.phase=CP_LK_NORMAL;b.link.epoch=8;b.link.tx_psn=99;
 b.link.owner.opaque=&b;b.link.owner.live=owner;
 b.link.owner.acked=ack_wireless;
 assert(cp_cloud_begin(&p,&b,&ops,8,4)==CP_CLOUD_OK);
 /* Single original-owner loop, no external sequencing of radio calls.
  * Start with phone not associated, then observe the actual fixture edge. */
 associated=0;
 assert(cp_cloud_drive_first_connect(&p,NULL,NULL,90)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_BOOTSTRAP_AUTH);
 assert(cp_cloud_drive_first_connect(&p,NULL,NULL,91)==CP_CLOUD_RETRY);
 assert(p.radio.phase==CP_RADIO_CONNECTING&&p.band==CP_RADIO_24&&ap_calls==1);
 assert(cp_cloud_drive_first_connect(&p,NULL,NULL,92)==CP_CLOUD_RETRY);
 assert(p.radio.phase==CP_RADIO_CONNECTING);
 associated=1;
 assert(cp_cloud_drive_first_connect(&p,NULL,NULL,93)==CP_CLOUD_RETRY);
 assert(p.handoff.phase==CP_HO_WIFI_ASSOCIATED&&!p.remote_phase);
 assert(mkdtemp(dir));
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                              dir,geteuid(),getegid())==0);
 strcpy(e.socket_path,rx.unix_path);
 e.generation=8;e.epoch=8;e.session=1234;e.airplay_port=5000;
 e.server_uid=geteuid();e.server_gid=getegid();
 e.si_owner_verified=r81_si_owner;e.dio_server_verified=r81_dio_server;
 assert(cp_cloud_bind_remote_dio(&p,&e,"../../bad")==CP_CLOUD_PROOF);
 MuCprxRemoteEndpoint bad=e;bad.epoch=9;
 assert(cp_cloud_bind_remote_dio(&p,&bad,"wlan0")==CP_CLOUD_PROOF);
 assert(cp_cloud_drive_first_connect(&p,&e,"wlan0",94)==CP_CLOUD_RETRY);
 assert(!p.native_dio&&!p.rx_service&&!p.airplay&&p.remote_phase==1);
 /* No ACK yet: even aborting the whole SI pipeline cannot assert DIO's
  * original singleton has been safely restored. */
 assert(cp_cloud_end(&p)==CP_CLOUD_RETRY);
 assert(cp_cloud_bind_remote_dio(&p,&e,"wlan0")==CP_CLOUD_PROOF);
 /* Actual remote START is sent and handled, without dereferencing DIO. */
 int acked=0;
 for(unsigned i=0;i<110&&!acked;i++){
  int r=cp_cloud_drive_first_connect(&p,&e,"wlan0",100+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  assert(mu_cprx_tick(&rx,100+i)==0);
  r=cp_cloud_drive_first_connect(&p,&e,"wlan0",100+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  acked=p.remote_start_confirmed;
 }
 assert(acked&&native_start_calls==1&&p.remote_phase==2);
 assert(cp_cloud_end(&p)==CP_CLOUD_RETRY); /* cannot claim remote cleanup */
 assert(!cp_cloud_is_active(&p)); /* no permanent cached remote proof */
 b.identity_accepted=1;
 b.wireless.availability_seen=1;b.wireless.wireless_available=1;
 /* DIO media 4 must NOT be reported as active even if the old iAP2
  * bootstrap has emitted 4301. */
 for(unsigned i=0;i<50;i++){
  int r=cp_cloud_drive_remote_dio(&p,300+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  assert(mu_cprx_tick(&rx,300+i)==0);
  r=cp_cloud_drive_remote_dio(&p,300+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  if(b.wireless_pending&&!b.link.outbound_pending)break;
 }
 assert(!cp_cloud_remote_live_at(&p,352));
 assert(b.wireless.start_sent&&b.wireless_pending);
 /* Authentic iAP2 link ACK envelope, delivered into the existing link. */
 unsigned seq=b.link.outbound_seq;
 uint8_t ack[9]={0xff,0x5a,0,9,0x40,0,0,0,0};
 ack[6]=(uint8_t)seq;ack[7]=(uint8_t)(seq>>8);ack[8]=sum8(ack,8);
 assert(cp_lk_feed_bytes(&b.link,ack,sizeof ack,360)==CP_LK_OK);
 assert(!b.wireless_pending&&!b.link.outbound_pending);
 /* First-connect normal path: inner MFi completes before media. The
  * remote bootstrap can retire Bluetooth without waiting for audio/HMI. */
 int handoff_ready=0;
 for(unsigned i=0;i<100&&!handoff_ready;i++){
  uint64_t now=361+i;
  int r=cp_cloud_drive_remote_dio(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  assert(mu_cprx_tick(&rx,now)==0);
  r=cp_cloud_drive_remote_dio(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  handoff_ready=p.remote_bootstrap_committed;
 }
 assert(handoff_ready&&p.handoff.phase==CP_HO_INNER_IAP_AUTH);
 assert(!cp_cloud_remote_video_at(&p,p.remote_sample_ms));
 assert(cp_cloud_bt_closed(&p,4)==CP_CLOUD_PROOF);
 bt=0;inner=0;
 assert(cp_cloud_bt_closed(&p,4)==CP_CLOUD_PROOF);
 inner=1;
 assert(cp_cloud_bt_closed(&p,4)==CP_CLOUD_OK&&!p.handoff.bt_connected);
 native_media_flag=7;
 for(int ch=2;ch<=4;ch++)native_channel[ch]=media[ch]=0;
 int video=0;uint64_t video_at=0;
 for(unsigned i=0;i<100&&!video;i++){
  uint64_t now=470+i;
  int r=cp_cloud_drive_first_connect(&p,&e,"wlan0",now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF||r==CP_CLOUD_OK);
  if(r==CP_CLOUD_OK){video=1;video_at=now;break;}
  assert(mu_cprx_tick(&rx,now)==0);
  r=cp_cloud_drive_first_connect(&p,&e,"wlan0",now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF||r==CP_CLOUD_OK);
  video=r==CP_CLOUD_OK;if(video)video_at=now;
 }
 assert(video&&!cp_cloud_remote_live_at(&p,video_at));
 assert(p.handoff.phase==CP_HO_INNER_IAP_AUTH&&p.remote_channels==1);
 for(int ch=2;ch<=4;ch++)native_channel[ch]=media[ch]=1;
 puts("PASS first-connect remote AirPlay: inner auth, BT release, video before optional channels");
 int got=0;uint64_t when=0;
 for(unsigned i=0;i<140&&!got;i++){
  uint64_t now=580+i;
  int r=cp_cloud_drive_remote_dio(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF||r==CP_CLOUD_OK);
  if(r==CP_CLOUD_OK){got=1;when=now;break;}
  assert(mu_cprx_tick(&rx,now)==0);
  r=cp_cloud_drive_remote_dio(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF||r==CP_CLOUD_OK);
  got=(r==CP_CLOUD_OK);
  if(got)when=now;
 }
 assert(got&&p.handoff.phase==CP_HO_MEDIA_ACTIVE);
 assert(p.remote_bootstrap_committed);
 assert(cp_cloud_remote_live_at(&p,when));
 /* Already retired during inner auth: original media remains live. */
 assert(!p.handoff.bt_connected&&cp_cloud_remote_video_at(&p,when));
 assert(cp_cloud_remote_live_at(&p,when));
 inner=0;assert(!cp_cloud_remote_live_at(&p,when));inner=1;
 assert(!cp_cloud_remote_live_at(&p,when+501)); /* stale status */
 assert(!cp_cloud_is_active(&p)); /* must call remote_live_at with clock */
 /* DIO steering dropped while a cached 0x0f existed; the independent SI
  * evidence rejects the same sample before the next IPC round. */
 media[CP_CLOUD_STEERING]=0;
 assert(!cp_cloud_remote_live_at(&p,when));
 media[CP_CLOUD_STEERING]=1;
 p.remote_dio.session=1235;
 assert(!cp_cloud_remote_live_at(&p,when));
 p.remote_dio.session=1234;
 /* A media=7 snapshot may NEVER authorize a voluntary STOP. */
 assert(cp_cloud_remote_graceful_end(&p,when)==CP_CLOUD_RETRY);
 native_media_flag=0;
 int idle=0;uint64_t idle_when=0;
 for(unsigned i=0;i<130&&!idle;i++){
  uint64_t now=730+i;
  int r=cp_cloud_drive_remote_dio(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF||r==CP_CLOUD_OK);
  idle=p.remote_snapshot_fresh&&p.remote_media==0;
  if(idle){idle_when=now;break;}
  int sr=mu_cprx_tick(&rx,now);assert(sr==0);
  r=cp_cloud_drive_remote_dio(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF||r==CP_CLOUD_OK);
  idle=p.remote_snapshot_fresh&&p.remote_media==0;
  if(idle)idle_when=now;
 }
 assert(idle);
 int ended=0;
 for(unsigned i=0;i<50&&!ended;i++){
  uint64_t now=idle_when+i;
  int r=cp_cloud_remote_graceful_end(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_OK);
  int sr=mu_cprx_tick(&rx,now);assert(sr==0||sr==1);
  r=cp_cloud_remote_graceful_end(&p,now);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_OK);
  ended=r==CP_CLOUD_OK;
 }
 assert(ended&&rx.backend_released&&p.remote_phase==4&&p.stopped);
 assert(native_stop_calls==1&&!native.initialized);
 assert(!strcmp(current_native_iface,"eth0"));
 assert(rmdir(dir)==0);dlclose(h);
}
/* R85: same OEM DIO singleton, but physically distinct SI vs DIO original
 * wired event witnesses. The SI cannot declare the DIO callback true. */
static int r85_si_cable=0,r85_dio_cable=0,r85_teardown_ok=0;
static int r85_final_teardown(void *ctx,uint32_t e){
 (void)ctx;return e==8&&r85_teardown_ok;
}
static int r85_si_wired_idle(void *opaque){(void)opaque;return !r85_si_cable;}
static int r85_si_wired(void *opaque,uint32_t gen,uint64_t epoch,uint32_t sess){
 (void)opaque;return r85_si_cable&&gen==8&&epoch==8&&sess==1234;
}
static int r85_dio_wired(void *opaque,uint32_t gen,uint64_t epoch,uint32_t sess){
 (void)opaque;return r85_dio_cable&&gen==8&&epoch==8&&sess==1234;
}
static void native_r85_wired_remote_rollback(void){
 struct cp_cloud_pipeline p;struct cp_step04_session b;struct cp_stock_mfi stock;
 MuDioNative native={0};MuCprxService rx={0};
 const MuDioGuard guard={NULL,native_lib,native_owner,native_listening,
                        native_stopped,native_interface};
 const MuCprxEvidence evidence={.admitted=rx_admitted,.identity=rx_identity,
  .media=rx_media,.si_admitted=rx_si,.si_client_verified=rx_peer,
  .si_inner_iap2_mfi=rx_inner,.si_media_channel=rx_channel,
  .dio_wired_takeover_verified=r85_dio_wired};
 struct cp_cloud_owner si_ops=ops;
 si_ops.wired_idle=r85_si_wired_idle;
 si_ops.teardown_confirmed=r85_final_teardown;
 MuCprxRemoteEndpoint e={0};
 char dir[]="/tmp/au57x-r85-wired-XXXXXX";
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 reset_fixture();native_owner_ok=1;native_listener=0;
 native_stop_pending=0;native_stop_calls=0;
 native_media_flag=7;native_inner=1;si_peer_allowed=1;
 r85_si_cable=r85_dio_cable=r85_teardown_ok=0;
 strcpy(current_native_iface,"eth0");
 mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 assert(cp_cloud_begin(&p,&b,&si_ops,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 assert(mkdtemp(dir));
 assert(mu_dio_bind_interface(&native,h,&airplay_object,&airplay_owner,
                              8,&guard,"eth0","wlan0")==0);
 assert(mu_cprx_open_si_unix(&rx,&native,&evidence,5000,8,1234,
                              dir,geteuid(),getegid())==0);
 char path[MU_CPRX_UNIX_PATH_MAX];strcpy(path,rx.unix_path);
 strcpy(e.socket_path,rx.unix_path);
 e.generation=8;e.epoch=8;e.session=1234;e.airplay_port=5000;
 e.server_uid=geteuid();e.server_gid=getegid();
 e.si_owner_verified=r81_si_owner;e.dio_server_verified=r81_dio_server;
 e.si_wired_takeover_verified=r85_si_wired;
 assert(cp_cloud_bind_remote_dio(&p,&e,"wlan0")==CP_CLOUD_OK);
 int started=0;
 for(int i=0;i<35&&!started;i++){
  int r=cp_cloud_drive_remote_dio(&p,100+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  assert(mu_cprx_tick(&rx,100+i)==0);
  r=cp_cloud_drive_remote_dio(&p,100+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_PROOF);
  started=p.remote_start_confirmed;
 }
 assert(started&&native_start_calls==1&&rx.started&&rx.start_confirmed);
 assert(native_media_flag==7&&native_stop_calls==0);
 /* Forged/disabled SI authorization is not enough to force DIO teardown. */
 r85_si_cable=1;
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_RETRY);
 assert(p.remote_wired_pending&&p.stopped&&p.handoff.wired_active);
 assert(!cp_cloud_remote_live_at(&p,300));
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_RETRY);
 p.remote_dio.si_wired_takeover_verified=NULL;
 assert(cp_cloud_drive_remote_dio(&p,300)==CP_CLOUD_RETRY);
 assert(p.remote_client.phase==0&&native_stop_calls==0);
 assert(cp_cloud_end(&p)==CP_CLOUD_RETRY);
 p.remote_dio.si_wired_takeover_verified=r85_si_wired;
 /* SI wired=true but independent ORIGINAL DIO wired=false is a denial.
  * An active iPhone media stream must continue until DIO independently
  * sees the actual wired event, not a mere SI-originated socket request. */
 int rejected=0;
 for(int i=0;i<25&&!rejected;i++){
  int r=cp_cloud_drive_remote_dio(&p,320+i);
  assert(r==CP_CLOUD_RETRY);
  assert(mu_cprx_tick(&rx,320+i)==0);
  r=cp_cloud_drive_remote_dio(&p,320+i);
  assert(r==CP_CLOUD_RETRY);
  rejected=p.remote_client.phase==0;
 }
 assert(rejected&&native_stop_calls==0&&native.initialized);
 struct stat st;assert(lstat(path,&st)==0&&S_ISSOCK(st.st_mode));
 /* Once BOTH original sides verify physical wired takeover, op12 can
  * restore despite media==7 (ordinary STOP still rejects). */
 r85_dio_cable=1;
 int restored=0,pending=0;
 for(int i=0;i<60&&!restored;i++){
  int r=cp_cloud_drive_remote_dio(&p,360+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_OK);
  int sr=mu_cprx_tick(&rx,360+i);
  assert(sr==0||sr==1);
  if(native_stop_calls&&rx.listener>=0){
   pending=1;
   assert(!rx.backend_released&&lstat(path,&st)==0&&S_ISSOCK(st.st_mode));
   assert(p.remote_phase!=4);
  }
  r=cp_cloud_drive_remote_dio(&p,360+i);
  assert(r==CP_CLOUD_RETRY||r==CP_CLOUD_OK);
  restored=rx.backend_released;
  assert(!restored||r==CP_CLOUD_RETRY); /* no SI teardown proof yet */
 }
 assert(restored&&pending&&rx.backend_released);
 assert(native_stop_calls==1&&!native.initialized);
 assert(p.remote_phase==4&&!p.remote_wired_pending&&p.stopped);
 assert(!p.remote_teardown_confirmed);
 assert(!strcmp(current_native_iface,"eth0"));
 assert(lstat(path,&st)==-1&&errno==ENOENT);
 /* DIO restored, but SI's final teardown witness is still pending. Do not
  * restart the original receiver or report complete until SI confirms. */
 assert(cp_cloud_drive_remote_dio(&p,440)==CP_CLOUD_RETRY);
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_RETRY); /* no false OK */
 assert(native_stop_calls==1&&!p.remote_teardown_confirmed);
 r85_teardown_ok=1;
 assert(cp_cloud_drive_remote_dio(&p,441)==CP_CLOUD_OK);
 assert(p.remote_teardown_confirmed&&p.handoff.phase==CP_HO_WIRED);
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK);
 assert(cp_cloud_drive_remote_dio(&p,442)==CP_CLOUD_STOPPED);
 assert(rmdir(dir)==0);dlclose(h);
 r85_si_cable=r85_dio_cable=r85_teardown_ok=0;
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
/* R87: a real ACL connection alone cannot authorize wireless. The
 * original R44 MFi owner is the only path into the R85 pipeline. */
static int wcp_live=0,wcp_opened=0;
static int wcp_owner(void *ctx,uint32_t gen){(void)ctx;return bt&&gen==4;}
static int wcp_paired(void *ctx,uint32_t gen){(void)ctx;return gen==4;}
static int wcp_wired(void *ctx){(void)ctx;return wired;}
static int wcp_link(void *ctx,uint32_t gen){(void)ctx;return gen==4&&wcp_live;}
static int wcp_sdp(void *ctx,uint32_t gen){(void)ctx;return gen==4;}
static int wcp_open(void *ctx,uint32_t gen){(void)ctx;wcp_opened++;return gen==4;}
static int wcp_cancel(void *ctx,uint32_t gen){(void)ctx;return gen==4;}
static void r87_cloud_bootstrap(void){
 struct cp_wcp_autoconnect a;struct cp_cloud_pipeline p;
 struct cp_step04_session b;struct cp_stock_mfi m;
 const struct cp_wcp_bt_ops w={NULL,wcp_owner,wcp_paired,wcp_wired,wcp_link,
                               wcp_sdp,wcp_open,wcp_cancel};
 reset_fixture();wcp_live=0;wcp_opened=0;
 mocked_oem(&b,&m,0);cp_cloud_init(&p);
 assert(cp_wcp_bt_init(&a,&w)==CP_WCP_OK);
 assert(cp_wcp_bt_acl_up(&a,4,0)==CP_WCP_OK);
 assert(cp_wcp_begin_cloud(&a,&b,&p,&ops)==CP_WCP_DENIED);
 assert(cp_wcp_bt_tick(&a,4,4000)==CP_WCP_WAIT);
 assert(wcp_opened==1);
 wcp_live=1;
 assert(cp_wcp_bt_tick(&a,4,4001)==CP_WCP_LINK_READY);
 assert(cp_wcp_begin_cloud(&a,&b,&p,&ops)==CP_WCP_WAIT);
 assert(!p.started); /* BT only != Apple MFi */
 mocked_oem(&b,&m,1);
 wired=0;assert(cp_wcp_begin_cloud(&a,&b,&p,&ops)==CP_WCP_DENIED);
 wired=1;
 assert(cp_wcp_begin_cloud(&a,&b,&p,&ops)==CP_WCP_OK);
 assert(p.started&&p.epoch==8&&p.bt_generation==4);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 assert(cp_wcp_begin_cloud(&a,&b,&p,&ops)==CP_WCP_DENIED);
 assert(cp_wcp_bt_abort(&a,4,1)==CP_WCP_OK);
 assert(cp_wcp_begin_cloud(&a,&b,&p,&ops)==CP_WCP_DENIED);
 reset_fixture();
}

int main(void){r87_cloud_bootstrap();successful_dual_fallback();only_24();no_cert_no_radio();radio_to_airplay_proof();automatic_native_carplay_start();native_dio_bound_pipeline();native_rx_media_contract();native_graceful_stop_protocol();native_graceful_pipeline_end();native_si_owner_loss_recovery();native_abandoned_mutation_rollback();native_remote_process_boundary();native_r83_corrupt_session_snapshot();native_remote_owner_cloud_pipeline();native_r85_wired_remote_rollback();
 puts("AU57X_R47_CLOUD_04_05_06_07_08_09_OWNER_GATES_PASS");return 0;}
