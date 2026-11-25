/* SPDX-License-Identifier: GPL-3.0-only
 * One cloud scenario. Real QNX code + socket transport; external radio,
 * MFi, and OEM receiver/media witnesses are fixture inputs, NOT OEM proof. */
#define main previous_pipeline_fixture_main
#include "../../tests/test_cloud_pipeline.c"
#undef main
#include "../../integration/mu1005_si_native/inner_iap2/mm_peer.h"
#include "../../integration/mu1005_si_native/inner_iap2/inner_socket.h"
static MuMmPeer chain_mm;
static MuInnerSocket chain_dio;
static int chain_device,chain_events,chain_verified;
static unsigned char chain_marker[]={255,85,2,0,238,16};
static unsigned char chain_received[6];
static size_t chain_used;
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
static int chain_inner(void *ctx,uint32_t e){
 (void)ctx;return e==8&&chain_verified&&chain_events==1&&chain_used==6;
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 struct cp_cloud_pipeline p;
 struct cp_step04_session b;
 struct cp_stock_mfi stock;
 Mu34ApServer ap_backend;
 struct cp_cloud_owner o=ops;
 o.inner_iap2_mfi_verified=chain_inner; /* TEST ONLY: transport proof, not MFi */
 reset_fixture();mocked_oem(&b,&stock,1);cp_cloud_init(&p);
 puts("CHAIN:BT_AND_MFI_FIXTURE");
 assert(cp_cloud_begin(&p,&b,&o,8,4)==CP_CLOUD_OK);
 assert(cp_cloud_confirm_bootstrap_auth(&p)==CP_CLOUD_OK);
 puts("CHAIN:RADIO_FIXTURE_5_TO_24");
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK&&p.band==CP_RADIO_5);
 assert(cp_cloud_radio_result(&p,0,0)==CP_CLOUD_RETRY);
 assert(cp_cloud_radio_result(&p,0,1)==CP_CLOUD_RETRY);
 assert(cp_cloud_radio_start(&p)==CP_CLOUD_OK&&p.band==CP_RADIO_24);
 assert(cp_cloud_radio_result(&p,1,0)==CP_CLOUD_OK);
 native_airplay(&ap_backend);
 assert(cp_cloud_bind_airplay(&p,&ap_backend)==CP_CLOUD_OK);
 assert(cp_cloud_prepare_receiver(&p,"wlan0")==CP_CLOUD_OK);
 static const struct cp_session cfg={.airplay_port=5000};
 b.wireless.session=&cfg;b.wireless_bound=1;b.wireless.start_sent=1;
 assert(cp_cloud_carplay_request(&p)==CP_CLOUD_OK);
 assert(cp_cloud_airplay_ready(&p,"wlan0")==CP_CLOUD_OK);
 puts("CHAIN:AIRPLAY_OWNER_FIXTURE_REAL_TRANSPORT_NEXT");
 assert(cp_cloud_inner_auth(&p)==CP_CLOUD_PROOF);
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
 assert(chain_used==6&&!memcmp(chain_received,chain_marker,6));
 chain_verified=1;
 puts("CHAIN:REAL_SOCKET_ROUND_TRIP");
 assert(cp_cloud_inner_auth(&p)==CP_CLOUD_OK);
 assert(cp_cloud_media_ready(&p)==CP_CLOUD_OK&&cp_cloud_is_active(&p));
 puts("CHAIN:MEDIA_FIXTURE");
 chain_verified=0;
 assert(!cp_cloud_is_active(&p));
 assert(cp_cloud_wired_preempt(&p)==CP_CLOUD_OK);
 assert(cp_cloud_end(&p)==CP_CLOUD_OK);
 assert(mu_mm_peer_release(&chain_mm)==0);
 mu_inner_socket_close(&chain_dio);
 puts("QNX_CHAIN_SIMULATED_HARDWARE_REAL_TRANSPORT_PASS_NO_OEM");
 return 0;
}
