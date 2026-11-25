/* SPDX-License-Identifier: GPL-3.0-only
 * One cloud scenario. Real QNX code + socket transport; external radio,
 * MFi, and OEM receiver/media witnesses are fixture inputs, NOT OEM proof. */
/* No stock uaputl is available in the QEMU fixture. Fail closed if called;
 * only the injected fixture query is used by the read-to-refresh test. */
int mhi2q_qnx_capture(const char *p,char *const a[],char *o,unsigned n,int *inc){
 (void)p;(void)a;(void)o;(void)n;if(inc)*inc=1;return -1;
}
#define main previous_pipeline_fixture_main
#include "../../tests/test_cloud_pipeline.c"
#undef main
#include "vehicle_profile.h"
#include "cp_wcp_autoconnect.h"
/* Lab owner observations, not a real BlueSDK adapter. Keep the same
 * controller across generations so stale/blocked state is exercised. */
struct chain_bt_fixture {uint32_t gen;int idle,linked,released,opens,cancels;};
static struct chain_bt_fixture chain_bt={4,1,0,0,0,0};
static struct cp_wcp_autoconnect chain_auto;
static int chain_bt_gen(void *v,uint32_t g){return ((struct chain_bt_fixture*)v)->gen==g;}
static int chain_bt_idle(void *v){return ((struct chain_bt_fixture*)v)->idle;}
static int chain_bt_link(void *v,uint32_t g){return chain_bt_gen(v,g)&&((struct chain_bt_fixture*)v)->linked;}
static int chain_bt_open(void *v,uint32_t g){assert(chain_bt_gen(v,g));++((struct chain_bt_fixture*)v)->opens;return 1;}
static int chain_bt_cancel(void *v,uint32_t g){assert(chain_bt_gen(v,g));++((struct chain_bt_fixture*)v)->cancels;return 1;}
static int chain_bt_released(void *v,uint32_t g){return g==4&&((struct chain_bt_fixture*)v)->released;}
static void chain_auto_first(void){
 const struct cp_wcp_bt_ops ops={&chain_bt,chain_bt_gen,chain_bt_gen,chain_bt_idle,
  chain_bt_link,chain_bt_gen,chain_bt_open,chain_bt_cancel};
 assert(cp_wcp_bt_init(&chain_auto,&ops)==CP_WCP_OK);
 assert(cp_wcp_bt_acl_up(&chain_auto,4,0)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&chain_auto,4,3999)==CP_WCP_WAIT&&chain_bt.opens==0);
 assert(cp_wcp_bt_tick(&chain_auto,4,4000)==CP_WCP_WAIT&&chain_bt.opens==1);
 chain_bt.linked=1;
 assert(cp_wcp_bt_tick(&chain_auto,4,4001)==CP_WCP_LINK_READY);
}
static void chain_auto_after_teardown(void){
 chain_bt.idle=0;
 assert(cp_wcp_bt_tick(&chain_auto,4,4002)==CP_WCP_DENIED);
 assert(chain_auto.state==CP_WCP_BLOCKED&&chain_bt.cancels==1);
 chain_bt.gen=5;chain_bt.linked=0;
 assert(cp_wcp_bt_rearm(&chain_auto,5,5000,chain_bt_released)==CP_WCP_DENIED);
 chain_bt.released=1; /* caller reached confirmed lab pipeline teardown */
 assert(cp_wcp_bt_rearm(&chain_auto,5,5000,chain_bt_released)==CP_WCP_DENIED);
 chain_bt.idle=1;
 assert(cp_wcp_bt_rearm(&chain_auto,4,5000,chain_bt_released)==CP_WCP_STALE);
 assert(cp_wcp_bt_rearm(&chain_auto,5,5000,chain_bt_released)==CP_WCP_OK);
 assert(chain_auto.attempts==0&&cp_wcp_bt_tick(&chain_auto,4,9000)==CP_WCP_STALE);
 assert(cp_wcp_bt_tick(&chain_auto,5,8999)==CP_WCP_WAIT&&chain_bt.opens==1);
 assert(cp_wcp_bt_tick(&chain_auto,5,9000)==CP_WCP_WAIT&&chain_bt.opens==2);
 chain_bt.linked=1;
 assert(cp_wcp_bt_tick(&chain_auto,5,9001)==CP_WCP_LINK_READY);
 puts("CHAIN:FRESH_ACL_RECONNECT_LINK_READY_NO_OEM");
 assert(cp_wcp_bt_abort(&chain_auto,5,0)==CP_WCP_OK&&chain_bt.cancels==2);
}
extern int chain_bootstrap_fixture(void);
#include "../../integration/mu1005_si_native/inner_iap2/mm_peer.h"
#include "../../integration/mu1005_si_native/inner_iap2/inner_socket.h"
#ifndef MU_CHAIN_MODEL_IPC
static MuMmPeer chain_mm;
#else
static MuInner chain_model;
static int model_owner_up=1;
#endif
#ifndef MU_CHAIN_MODEL_IPC
static MuInnerSocket chain_dio;
static int chain_device;
#endif
static int chain_events,chain_verified;
static unsigned char chain_marker[]={255,85,2,0,238,16};
static unsigned char chain_received[6];
static size_t chain_used;
#ifndef MU_CHAIN_MODEL_IPC
static int chain_mproof(void *ctx,void *dev,int fd,uint64_t e){
 return ctx==&chain_mm&&dev==&chain_device&&fd>=0&&e==8;
}
static int chain_dproof(void *ctx,int fd,uint64_t e){
 return ctx==&chain_dio&&fd>=0&&e==8;
}
static int chain_notice(void *ctx,void *dev){
 assert(ctx==&chain_mm&&dev==&chain_device);
 int n=mu_mm_peer_recv(dev,chain_received+chain_used,2,0,NULL);
 assert(n>0&&chain_used+(size_t)n<=sizeof chain_received);
 chain_used+=(size_t)n;return 1;
}
static int chain_event(void *ctx,const unsigned char *p,size_t n){
 assert(ctx==&chain_dio&&n==6&&!memcmp(p,chain_marker,6));
 ++chain_events;return 1;
}
#else
/* Explicit lab-only replacement for unavailable OS socket service.
 * Exercise unchanged production frame parser/pump, not a fake success flag. */
static int model_live(void *ctx,uint64_t e){return model_owner_up&&ctx==&chain_model&&e==8;}
static int model_event(void *ctx,const unsigned char *p,size_t n){
 assert(ctx==&chain_model&&n==6&&!memcmp(p,chain_marker,n));
 ++chain_events;return 1;
}
static int model_receive(void *ctx,const unsigned char *p,size_t n){
 assert(ctx==&chain_model&&n&&chain_used+n<=sizeof chain_received);
 if(n>2)n=2;
 memcpy(chain_received+chain_used,p,n);chain_used+=n;return (int)n;
}
#endif
static int chain_inner(void *ctx,uint32_t e){
 (void)ctx;
#ifndef MU_CHAIN_MODEL_IPC
 if(!chain_dio.relay.active||chain_dio.fd<0||!chain_mm.active)return 0;
#else
 if(!chain_model.active||!model_owner_up)return 0;
#endif
 return e==8&&chain_verified&&chain_events==1&&chain_used==6;
}
static struct cp_cloud_pipeline p;
static Mu34ApServer ap_backend;
static struct cp_cloud_owner o;
static int chain_bonjour(void *ctx,uint32_t e,const char *iface){
 (void)ctx;return bonjour&&e==8&&!strcmp(iface,MU_LAB_AP_IFACE);
}
static int chain_listener(void *ctx,uint32_t e,const char *iface,unsigned port){
 (void)ctx;return listener&&e==8&&port==MU_LAB_AP_PORT&&!strcmp(iface,MU_LAB_AP_IFACE);
}
static int chain_session(void *ctx,uint32_t e,const char *iface,unsigned port){
 (void)ctx;return phone_session&&e==8&&port==MU_LAB_AP_PORT&&!strcmp(iface,MU_LAB_AP_IFACE);
}
static int chain_cf(void *ctx,const void *cf,char *out,size_t cap){
 (void)ctx;(void)cf;
 if(cap<sizeof MU_LAB_AP_IFACE)return -1;
 memcpy(out,MU_LAB_AP_IFACE,sizeof MU_LAB_AP_IFACE);return sizeof MU_LAB_AP_IFACE-1;
}
void chain_after_identification(struct cp_step04_session *b){
 reset_fixture();o=ops;
 o.inner_iap2_mfi_verified=chain_inner; /* lab-only inner transport witness */
 o.bonjour_identity_verified=chain_bonjour;
 o.airplay_listener_verified=chain_listener;
 o.airplay_session_connected=chain_session;
 cp_cloud_init(&p);
 assert(cp_s4_authenticated(b)&&b->identity_accepted);
 assert(b->wireless.session->wifi.channel==MU_LAB_AP_CHANNEL);
 assert(cp_cloud_begin(&p,b,&o,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 /* Match actual R106 AP channel rather than pretending 5 GHz was observed. */
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK&&p.band==CP_RADIO_24);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 puts("CHAIN:R106_AP_UAP0_CHANNEL11_ASSOCIATION_FIXTURE");
 const Mu34ApServerBinding bind={stock_owner,cf_ok,chain_cf,stock_owner,
  setprop,control_ap,confirmed_ap,NULL,&airplay_object,&airplay_owner,
  &airplay_interface_key,&airplay_start_key,&airplay_stop_key,8};
 assert(mu34_ap_server_init(&ap_backend,&bind)==0);
 assert(cp_cloud_bind_airplay(&p,&ap_backend)==CP_CLOUD_OK);
 assert(cp_cloud_prepare_receiver(&p,MU_LAB_AP_IFACE)==CP_CLOUD_OK);
 assert(cp_cloud_carplay_request(&p)==CP_CLOUD_PROOF);
}
void chain_after_start_ack(struct cp_step04_session *b){
 assert(b==p.bootstrap&&b->wireless.start_sent&&!b->wireless_pending);
 assert(cp_cloud_carplay_request(&p)==CP_CLOUD_OK);
 assert(cp_cloud_airplay_ready(&p,MU_LAB_AP_IFACE)==CP_CLOUD_OK);
 puts("CHAIN:AIRPLAY_OWNER_FIXTURE_REAL_TRANSPORT_NEXT");
 assert(cp_cloud_inner_auth(&p)==CP_CLOUD_PROOF);
#ifndef MU_CHAIN_MODEL_IPC
 int f[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,f)==0);
 for(int i=0;i<2;i++)assert(fcntl(f[i],F_SETFL,O_NONBLOCK)==0);
 assert(mu_mm_peer_adopt(&chain_mm,&chain_device,f[0],8,&chain_mm,chain_mproof,chain_notice)==1);
 assert(mu_inner_socket_adopt(&chain_dio,f[1],8,&chain_dio,chain_dproof,chain_event)==1);
 assert(mu_mm_peer_send(&chain_device,chain_marker,6)==6);
 assert(mu_mm_peer_tick(&chain_mm)==1);
 assert(mu_inner_socket_tick(&chain_dio,8)==1);
 assert(mu_inner_socket_tick(&chain_dio,8)==1&&chain_events==1);
 assert(mu_inner_from_control(&chain_dio.relay,8,chain_marker,6)==1);
 assert(mu_inner_socket_tick(&chain_dio,8)==1);
 for(int i=0;i<3;i++)assert(mu_mm_peer_tick(&chain_mm)==1);
 puts("CHAIN:REAL_SOCKET_ROUND_TRIP");
#else
 MuInnerOps mo={&chain_model,model_live,model_event,model_receive};
 assert(mu_inner_start(&chain_model,8,&mo)==1);
 assert(mu_inner_from_cinemo(&chain_model,8,chain_marker,2)==1);
 assert(mu_inner_pump(&chain_model,8)==1&&chain_events==0);
 assert(mu_inner_from_cinemo(&chain_model,8,chain_marker+2,4)==1);
 assert(mu_inner_pump(&chain_model,8)==1&&chain_events==1);
 assert(mu_inner_from_control(&chain_model,8,chain_marker,6)==1);
 for(int i=0;i<3;i++)assert(mu_inner_pump(&chain_model,8)==1);
 puts("CHAIN:MODEL_IPC_REAL_PACKET_RELAY");
#endif
 assert(chain_used==6&&!memcmp(chain_received,chain_marker,6));
 chain_verified=1;
 assert(cp_cloud_inner_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_media_ready(&p)==CP_CLOUD_OK&&cp_cloud_is_active(&p));
 puts("CHAIN:MEDIA_FIXTURE");
 /* Lose the actual transport, not just a fixture boolean. The live
  * predicate must invalidate media even while chain_verified stays set. */
#ifndef MU_CHAIN_MODEL_IPC
 assert(mu_mm_peer_release(&chain_mm)==0);
 assert(mu_inner_socket_tick(&chain_dio,8)==-1);
 assert(chain_dio.fd==-1&&!chain_dio.relay.active);
 assert(mu_inner_from_control(&chain_dio.relay,8,chain_marker,6)==-1);
 assert(mu_mm_peer_send(&chain_device,chain_marker,6)==-1);
 puts("CHAIN:REAL_PEER_EOF_REVOKED_OLD_SESSION");
#else
 model_owner_up=0;
 assert(mu_inner_pump(&chain_model,8)==-1&&!chain_model.active);
 assert(mu_inner_from_control(&chain_model,8,chain_marker,6)==-1);
 puts("CHAIN:MODEL_OWNER_LOSS_REVOKED_OLD_SESSION");
#endif
 assert(chain_verified&&chain_events==1&&chain_used==6);
 assert(!cp_cloud_is_active(&p));
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK&&!cp_cloud_is_active(&p));
 assert(cp_cloud_end(&p)==CP_CLOUD_OK);
 chain_verified=0;
 puts("CHAIN:WIRED_RESTORE_AFTER_INNER_LOSS");
 chain_auto_after_teardown();
#ifndef MU_CHAIN_MODEL_IPC
 mu_inner_socket_close(&chain_dio);
 puts("CHAIN_IAP2_BYTES_AND_SOCKET_ROUND_TRIP_PASS_NO_OEM");
#else
 mu_inner_end(&chain_model);
 puts("CHAIN_IAP2_BYTES_AND_MODEL_IPC_PASS_NO_OEM");
#endif
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 /* Run the same production-path AP snapshot tests on host and QNX. */
 radio_to_airplay_proof();
 chain_auto_first();
 return chain_bootstrap_fixture();
}
