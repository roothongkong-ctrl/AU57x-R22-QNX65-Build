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
static const uint8_t bt_id[6]={0x00,0x11,0x22,0x33,0x44,0x55},device_id[6]={0x02,0,0x22,0x44,0x66,0x88};
static const char ie_ok[]="Querying custom IE successful\nIndex [2]\nManagement Subtype Mask = 0x12a\nIE Buffer: len=26\ndd 18 00 a0 40 00 00 02 80 22 06 06 00 11 22 33\n44 55 07 06 02 00 22 44 66 88\n";
static int ie_query(void *ctx,unsigned id,char *out,size_t cap){
 (void)ctx;assert(id==6&&sizeof ie_ok<=cap);memcpy(out,ie_ok,sizeof ie_ok);return 0;
}
static const char ie_empty[]="Querying custom IE successful\nIndex [2]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\n";
struct ie_fixture {int installed,writes,allowed,fail;};
static int ie_state_query(void *v,unsigned id,char *out,size_t cap){
 struct ie_fixture *f=v;const char *p=f->installed?ie_ok:ie_empty;
 assert(id==6&&strlen(p)<cap);strcpy(out,p);return 0;
}
static int ie_write(void *v,unsigned slot,const uint8_t *bt,const uint8_t *id,int enable){
 struct ie_fixture *f=v;assert(slot==2&&!memcmp(bt,bt_id,6)&&!memcmp(id,device_id,6));
 f->writes++;if(f->fail)return -1;f->installed=enable;return 0;
}
static int exclusive(void *v,const uint8_t *bssid,unsigned slot){
 struct ie_fixture *f=v;return f->allowed&&slot==2&&bssid[0]==2;
}
static void ie_lifecycle(const struct mu_ap_live *ap){
 struct ie_fixture f={0};struct mu_ap_ie_lease lease={0};
 struct mu_ap_ie_ops o={&f,ie_state_query,ie_write,exclusive};
 assert(mu_ap_ie_install(&lease,&o,ap,2,bt_id,device_id)==-1&&!f.writes);
 f.allowed=1;f.installed=1;
 assert(mu_ap_ie_install(&lease,&o,ap,2,bt_id,device_id)==-1&&!f.writes);
 f.installed=0;
 assert(mu_ap_ie_install(&lease,&o,ap,2,bt_id,device_id)==0&&lease.state==1&&f.writes==1);
 assert(mu_ap_ie_restore(&lease,&o)==0&&!lease.state&&!f.installed&&f.writes==2);
 f.fail=1;
 assert(mu_ap_ie_install(&lease,&o,ap,2,bt_id,device_id)==-1&&lease.state==2);
 assert(mu_ap_ie_restore(&lease,&o)==-1&&f.writes==3);
 assert(mu_ap_ie_install(&lease,&o,ap,2,bt_id,device_id)==-1&&f.writes==3);
}
int main(void){
 struct mu_ap_live ap;struct cp_session s;
 const char*const ip[]={"10.0.0.1"};
 struct cp_session receiver={.ip_addresses=ip,.ip_address_count=1,.airplay_port=5000,.public_key="fixture",.source_version="fixture"};
 int calls=0;assert(mu_ap_live_read(&ap,q,&calls)==0&&calls==12&&ap.channel==6);
 assert(mu_ap_live_session(&s,&ap,&receiver)==0&&s.wifi.channel==6&&s.wifi.ssid==ap.ssid&&s.wifi.passphrase==ap.key&&s.wifi.bssid6==ap.bssid);
 ie_lifecycle(&ap);
 assert(mu_ap_live_verify_device_ie(&ap,ie_query,NULL,2,bt_id,device_id)==1);
 assert(mu_ap_device_ie_verify(ie_ok,1,bt_id,device_id)==0);
 assert(mu_ap_device_ie_verify(ie_ok,4,bt_id,device_id)==-1);
 char bad_ie[sizeof ie_ok+16];strcpy(bad_ie,ie_ok);
 char *mask=strstr(bad_ie,"0x12a");assert(mask);mask[4]='b';
 assert(mu_ap_device_ie_verify(bad_ie,2,bt_id,device_id)==-1);
 strcpy(bad_ie,ie_ok);strcat(bad_ie,"ff\n");
 assert(mu_ap_device_ie_verify(bad_ie,2,bt_id,device_id)==-1);
 uint8_t other[6]={2,1,2,3,4,5};
 assert(mu_ap_device_ie_verify(ie_ok,2,bt_id,other)==-1);
 struct mu_ap_live five=ap;five.channel=36;
 assert(mu_ap_live_verify_device_ie(&five,ie_query,NULL,2,bt_id,device_id)==-1);
 unsigned char b[1024];size_t n;assert(cp_encode_wifi_5703(&s.wifi,b,sizeof b,&n)==0&&b[4]==0x57&&b[5]==3);
 assert(cp_encode_start_4301(&s,b,sizeof b,&n)==0&&b[4]==0x43&&b[5]==1);
 /* Direct callers must not bypass the same snapshot validation as uaputl. */
 struct mu_ap_live bad=ap;
 bad.channel=15;assert(mu_ap_live_session(&s,&bad,&receiver)<0);
 bad=ap;bad.channel=36;assert(mu_ap_live_session(&s,&bad,&receiver)==0&&s.wifi.channel==36);
 bad=ap;memset(bad.bssid,0,6);assert(mu_ap_live_session(&s,&bad,&receiver)<0);
 bad=ap;bad.bssid[0]=1;assert(mu_ap_live_session(&s,&bad,&receiver)<0);
 bad=ap;memset(bad.key,'z',64);bad.key[64]=0;assert(mu_ap_live_session(&s,&bad,&receiver)<0);
 bad=ap;memset(bad.key,'a',64);bad.key[64]=0;
 /* Valid hexadecimal PSK passes local validation; the stock encoder may impose additional rules. */
 bad=ap;bad.key[1]='\x01';assert(mu_ap_live_session(&s,&bad,&receiver)<0);
 mu_ap_live_clear(&bad,sizeof bad);
 receiver.public_key=NULL;assert(mu_ap_live_session(&s,&ap,&receiver)<0);
 mu_ap_live_clear(&ap,sizeof ap);for(size_t i=0;i<sizeof ap;i++)assert(((unsigned char*)&ap)[i]==0);
 puts("PASS stock output fixture -> live snapshot -> existing 5703/4301 encoders; no physical AP");
 return 0;
}
