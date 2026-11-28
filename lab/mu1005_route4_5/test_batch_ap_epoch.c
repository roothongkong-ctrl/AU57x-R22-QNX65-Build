/* Fixture owners only; real existing AP snapshot and IE adapter. */
#include "ap_refresh.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint8_t bt[6]={2,4,6,8,10,12},id[6]={2,0,34,68,102,136};
typedef struct{struct cp_cloud_pipeline*p;int mode,writes,installed,queries;} F;
int cp_s4_authenticated(struct cp_step04_session*s){return s!=NULL;}
int cp_cloud_refresh_ap_config(struct cp_cloud_pipeline*p,const struct cp_session*s,int(*v)(void*,uint32_t,enum cp_radio_band,const struct cp_session*),void*c){(void)p;(void)s;(void)v;(void)c;assert(0);return -1;}
int mhi2q_qnx_capture(const char*p,char*const a[],char*b,unsigned n,int*i){(void)p;(void)a;(void)b;(void)n;(void)i;assert(0);return -1;}
static int wired(void*p){(void)p;return 1;}
static int alive(void*p,uint32_t e,uint32_t g){(void)p;return e&&g;}
static int query(void*v,unsigned n,char*out,size_t cap){
 F*f=v;f->queries++;
 const char*const a[]={"BSS status = started\n","SSID = LAB\n","Channel = 11\nBand    = 2.4GHz\n","AP MAC address = 02:00:00:00:00:01\n","PROTOCOL = WPA2\n","WPA passphrase = fixture-pass\n"};
 const char*msg;
 if(n<6){msg=a[n];if(f->mode==0&&f->queries==6){f->p->epoch++;f->p->radio.epoch++;}
 if(n==5&&f->installed)msg="WPA passphrase = changed-pass\n";}
 else if(f->installed)msg="Querying custom IE successful\nIndex [2]\nManagement Subtype Mask = 0x12a\nIE Buffer: len=26\ndd 18 00 a0 40 00 00 02 80 22 06 06 02 04 06 08 0a 0c 07 06 02 00 22 44 66 88\n";
 else msg="Querying custom IE successful\nIndex [2]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\n";
 assert(strlen(msg)<cap);strcpy(out,msg);return 0;
}
static int exclusive(void*v,const uint8_t*b,unsigned slot){(void)v;return b[0]==2&&slot==2;}
static int write_ie(void*v,unsigned slot,const uint8_t*b,const uint8_t*i,int en){F*f=v;assert(slot==2&&!memcmp(b,bt,6)&&!memcmp(i,id,6)&&en==1);f->installed=1;f->writes++;return 0;}
int main(int argc,char**argv){
 assert(argc==2);int mode=atoi(argv[1]);assert(mode==0||mode==1);
 struct cp_cloud_pipeline p={0};struct cp_step04_session boot={0};struct cp_session old={0};
 struct cp_identification identity={.wireless_ssid="LAB",.bluetooth_mac6=bt};
 struct mu_ap_refresh_slot s={0};struct mu_ap_ie_lease lease={0};
 F f={&p,mode,0,0,0};struct mu_ap_ie_ops o={&f,query,write_ie,exclusive};
 p.bootstrap=&boot;p.started=1;p.epoch=8;p.bt_generation=7;p.band=CP_RADIO_24;
 p.radio.epoch=8;p.radio.phase=CP_RADIO_CONNECTING;p.radio.selected=p.band;
 p.owner.ctx=&f;p.owner.wired_idle=wired;p.owner.bt_alive=alive;
 boot.wireless_bound=1;boot.wireless.session=&old;boot.identity=&identity;
 int r=mu_ap_refresh_prepare_first_link(&s,&p,&lease,&o,bt,id);
 assert(!s.prepared&&!s.ap.key[0]&&!p.live_ap_config);
 if(mode==0)assert(r==CP_CLOUD_STALE&&!lease.state&&!f.writes);
 else assert(r==CP_CLOUD_PROOF&&lease.state==1&&f.writes==1);
 printf("AP_EPOCH_OR_KEY_CHANGE_%d_PASS_FIXTURE_ONLY\n",mode);
}
