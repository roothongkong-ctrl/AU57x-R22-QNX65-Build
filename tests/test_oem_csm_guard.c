/* Host-only checks for OEM proof gating and iAP2 CSM policy.
 * No actual MFi, Bluetooth radio, iPhone or DIO process is used.
 */
#include "cp_oem_boundary.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct sink { unsigned count; unsigned last_id; int fail; };
static int capture(void *ctx,const uint8_t *data,size_t n) {
 struct sink *s=(struct sink *)ctx;
 assert(n>=6 && data[0]==0x40 && data[1]==0x40);
 assert(((unsigned)data[2]<<8|data[3])==n);
 if(s->fail)return -1;
 s->count++;
 s->last_id=((unsigned)data[4]<<8)|data[5];
 return 0;
}
int main(void){
 const char *ip[]={"192.0.2.9"};
 struct cp_session session={0};
 session.wifi.ssid="TEST_ONLY";session.wifi.passphrase="fake-password";
 session.wifi.security=2;session.wifi.channel=36;
 session.ip_addresses=ip;session.ip_address_count=1;
 session.airplay_port=7000;session.device_identifier="test-device";
 session.public_key="NOT_A_REAL_PUBLIC_KEY";session.source_version="host-test";
 struct cp_wireless w;struct sink sink={0};
 struct cp_oem_assertions p={0};
 /* Genuine complete 0x5702 CSM frame; this is a synthetic host fixture. */
 const uint8_t wifi_request[]={0x40,0x40,0,6,0x57,0x02};
 const uint8_t available[]={0x40,0x40,0,15,0x43,0,
                             0,9,0,1,0,5,0,0,1};
 const uint8_t malformed[]={0x40,0x40,0,8,0x57,0x02};
 cp_wireless_init(&w,&session,capture,&sink);
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_NOT_READY);
 assert(sink.count==0);
 p.verified_mfi=1;p.verified_control_session=1;p.live_hotspot=1;
 p.network_airplay_ready=1;p.sole_dio_owner=1;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_OK);
 assert(sink.count==1 && sink.last_id==0x5703);
 p.wired_active=1;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_NOT_READY);
 assert(sink.count==1);
 p.wired_active=0;p.sole_dio_owner=0;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_NOT_READY);
 p.sole_dio_owner=1;p.verified_mfi=0;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_NOT_READY);
 p.verified_mfi=1;p.verified_control_session=0;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_NOT_READY);
 p.verified_control_session=1;p.live_hotspot=0;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_NOT_READY);
 p.live_hotspot=1;p.network_airplay_ready=0;
 assert(cp_oem_dispatch(&w,&p,available,sizeof available)==CP_WL_NOT_READY);
 assert(sink.count==1);
 p.network_airplay_ready=1;
 assert(cp_oem_dispatch(&w,&p,available,sizeof available)==CP_WL_OK);
 assert(sink.count==2 && sink.last_id==0x4301);
 assert(cp_oem_dispatch(&w,&p,malformed,sizeof malformed)==CP_WL_BAD_FRAME);
 assert(sink.count==2);
 sink.fail=1;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_TRANSPORT_ERROR);
 assert(sink.count==2);
 sink.fail=0;
 assert(cp_oem_dispatch(&w,&p,wifi_request,sizeof wifi_request)==CP_WL_OK);
 assert(sink.count==3 && sink.last_id==0x5703);
 puts("AU57X_CLOUD12_OEM_CSM_FAIL_CLOSED_PASS");
 return 0;
}
