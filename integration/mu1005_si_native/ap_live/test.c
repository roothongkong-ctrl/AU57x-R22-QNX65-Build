#include "ap_live.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int q(void*ctx,unsigned id,char*out,size_t cap){
 const char*const a[]={"BSS status = started\n","SSID = AP_FIXTURE\n","Channel = 6\nBand    = 2.4GHz\n","AP MAC address = 02:00:00:00:00:01\n","PROTOCOL = WPA2 \n","WPA passphrase = fixture-only\n"};
 int *calls=ctx;(*calls)++;
 const char*p=a[id];
 assert(strlen(p)<cap);strcpy(out,p);return 0;
}
int main(void){
 struct mu_ap_live ap;struct cp_session s;
 const char*const ip[]={"10.0.0.1"};
 struct cp_session receiver={.ip_addresses=ip,.ip_address_count=1,.airplay_port=5000,.public_key="fixture",.source_version="fixture"};
 int calls=0;assert(mu_ap_live_read(&ap,q,&calls)==0&&calls==12&&ap.channel==6);
 assert(mu_ap_live_session(&s,&ap,&receiver)==0&&s.wifi.channel==6&&s.wifi.ssid==ap.ssid&&s.wifi.passphrase==ap.key&&s.wifi.bssid6==ap.bssid);
 unsigned char b[1024];size_t n;assert(cp_encode_wifi_5703(&s.wifi,b,sizeof b,&n)==0&&b[4]==0x57&&b[5]==3);
 assert(cp_encode_start_4301(&s,b,sizeof b,&n)==0&&b[4]==0x43&&b[5]==1);
 receiver.public_key=NULL;assert(mu_ap_live_session(&s,&ap,&receiver)<0);
 mu_ap_live_clear(&ap,sizeof ap);for(size_t i=0;i<sizeof ap;i++)assert(((unsigned char*)&ap)[i]==0);
 puts("PASS stock output fixture -> live snapshot -> existing 5703/4301 encoders; no physical AP");
 return 0;
}
