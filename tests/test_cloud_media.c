/* Host-only owner/sink adapter with synthetic media buffers, not a decoder. */
#include "cp_cloud_media.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int allow_owner=1,allow_source=1,sink_result=1;
static int wired=1,si_owner=1;
static int ext_wired(void*c){(void)c;return wired;}
static int ext_si(void*c,uint32_t e){(void)c;return si_owner&&e==8;}
static int ok(void *c,uint32_t e){(void)c;return allow_owner&&e==8;}
static int src(void*c,uint32_t e,enum cp_cloud_media m,uint32_t seq){
 (void)c;return allow_source&&e==8&&m>=1&&m<=4&&seq>0;
}
static int deliver(void*c,uint32_t e,enum cp_cloud_media m,
                   const uint8_t*p,size_t n,uint32_t seq){
 (void)c;assert(e==8&&m>=1&&m<=4&&p&&n&&seq);return sink_result;
}
int main(void){
 struct cp_cloud_pipeline cp;struct cp_cloud_media_router r;
 struct cp_cloud_media_ops ops={0,ok,src,deliver};
 Mu34ApServer ap;
 static const uint8_t sample[]={1,2,3};
 memset(&cp,0,sizeof cp);memset(&ap,0,sizeof ap);
 cp.epoch=8;cp.started=1;cp.handoff.phase=CP_HO_AIRPLAY_SESSION;
 cp.owner.wired_idle=ext_wired;cp.owner.si_single_owner=ext_si;
 cp.airplay=&ap;ap.generation=8;ap.state=MU34_AP_LISTENING_VERIFIED;
 assert(cp_cloud_media_bind(&r,&cp,&ops)==CP_FWD_OK);
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,8)==CP_FWD_OWNER);
 cp.handoff.phase=CP_HO_INNER_IAP_AUTH;
 assert(cp_cloud_media_bind(&r,&cp,&ops)==CP_FWD_OK);
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,7)==CP_FWD_STALE);
 /* Test-only raw media routing must reject native OEM decoder ownership. */
 MuDioNative native={0};
 cp.native_dio=&native;
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,8)==CP_FWD_OWNER);
 assert(cp_cloud_media_bind(&r,&cp,&ops)==CP_FWD_OWNER);
 cp.native_dio=NULL;
 assert(cp_cloud_media_bind(&r,&cp,&ops)==CP_FWD_OK);
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,8)==CP_FWD_OK);
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,8)==CP_FWD_ORDER);
 sink_result=0;
 assert(cp_cloud_media_forward(&r,CP_CLOUD_AUDIO,sample,sizeof sample,1,8)==CP_FWD_BACKPRESSURE);
 sink_result=1;
 assert(cp_cloud_media_forward(&r,CP_CLOUD_AUDIO,sample,sizeof sample,1,8)==CP_FWD_OK);
 for(int kind=CP_CLOUD_HMI;kind<=CP_CLOUD_STEERING;kind++)
  assert(cp_cloud_media_forward(&r,(enum cp_cloud_media)kind,sample,sizeof sample,1,8)==CP_FWD_OK);
 allow_source=0;
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,2,8)==CP_FWD_OWNER);
 allow_source=1;
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,3,8)==CP_FWD_ORDER);
 assert(cp_cloud_media_bind(&r,&cp,&ops)==CP_FWD_OK);
 wired=0;
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,8)==CP_FWD_OWNER);
 wired=1;si_owner=0;
 assert(cp_cloud_media_bind(&r,&cp,&ops)==CP_FWD_OK);
 assert(cp_cloud_media_forward(&r,CP_CLOUD_VIDEO,sample,sizeof sample,1,8)==CP_FWD_OWNER);
 puts("AU57X_R49_CLOUD_MEDIA_CONTROL_OWNER_ROUTER_PASS");return 0;
}
