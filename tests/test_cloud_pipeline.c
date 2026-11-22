/* AU57x owner-bound R04-09 cloud integration fixture: no OEM keys or RF. */
#include "cp_cloud_pipeline.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int wired=1,bt=1,cap5=1,cap24=1,ap=1,associated=1;
static int single=1,bonjour=1,listener=1,inner=1,request=1;
static int media[5]={0,1,1,1,1};
static unsigned ap_calls=0,observed_port=0,native_start_calls=0;
static int airplay_object,airplay_owner,airplay_start_key,airplay_stop_key,airplay_interface_key;
static int owner(void*ctx,uint32_t e,uint32_t g){(void)ctx;return bt&&e==8&&g==4;}
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
 ap_closed,cp_start,si,bonjour_ok,airplay_ok,inner_ok,media_ok,torn,running_ap};
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
 wired=bt=cap5=cap24=ap=associated=single=bonjour=listener=inner=request=1;
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
int main(void){successful_dual_fallback();only_24();no_cert_no_radio();radio_to_airplay_proof();
 puts("AU57X_R47_CLOUD_04_05_06_07_08_09_OWNER_GATES_PASS");return 0;}
