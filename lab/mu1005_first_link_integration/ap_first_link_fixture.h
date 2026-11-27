/* Host test providers only; never an OEM owner or radio driver. */
#include "ap_refresh.h"
static const uint8_t ap_bt[6]={2,4,6,8,10,12},ap_id[6]={2,0,34,68,102,136};
static const char ap_ie_ok[]="Index [2]\nManagement Subtype Mask = 0x12a\nIE Buffer: len=26\ndd 18 00 a0 40 00 00 02 80 22 06 06 02 04 06 08 0a 0c 07 06 02 00 22 44 66 88\n";
static const char ap_ie_empty[]="Index [2]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\n";
struct ap_fixture{int mode,writes,installed,allowed,proof,ie_queries;};
int mhi2q_qnx_capture(const char*p,char*const a[],char*b,unsigned n,int*i){
 (void)p;(void)a;(void)b;(void)n;*i=1;return -1;
}
static int ap_query(void*v,unsigned n,char*out,size_t cap){
 struct ap_fixture*f=v;const char *s=NULL;
 const char*const data[]={"BSS status = started\n","SSID = AU57X-LAB\n","Channel = 11\nBand    = 2.4GHz\n","AP MAC address = 02:00:00:00:00:01\n","PROTOCOL = WPA2 \n","WPA passphrase = refreshed-fixture\n"};
 if(n<6){s=data[n];if(n==2&&f->mode==3)s="Channel = 36\nBand    = 5GHz\n";
  if(n==1&&f->mode==7)s="SSID = OTHER-AP\n";
  assert(strlen(s)<cap);strcpy(out,s);return 0;}
 assert(n==6);f->ie_queries++;
 if(f->mode==2)s="";
 else if(f->mode==4&&f->ie_queries>1)s=ap_ie_ok;
 else s=f->installed?ap_ie_ok:ap_ie_empty;
 int z=snprintf(out,cap,"Querying custom IE successful\n%s",s);return z>0&&(size_t)z<cap?0:-1;
}
static int ap_write(void*v,unsigned slot,const uint8_t*b,const uint8_t*i,int en){
 struct ap_fixture*f=v;assert(slot==2&&!memcmp(b,ap_bt,6)&&!memcmp(i,ap_id,6));f->writes++;f->installed=en;return 0;
}
static int ap_exclusive(void*v,const uint8_t*b,unsigned slot){return ((struct ap_fixture*)v)->allowed&&b[0]==2&&slot==2;}
static int ap_verify(void*v,uint32_t e,uint32_t g,enum cp_radio_band band,const struct mu_ap_live*a){
 struct ap_fixture*f=v;return f->proof&&f->installed&&e==8&&g==7&&band==CP_RADIO_24&&a->channel==11;
}
static void ap_handoff(struct cp_cloud_pipeline*p,struct mu_ap_refresh_slot*s,struct mu_ap_ie_lease*l,struct ap_fixture*f){
 const char *env=getenv("AP_CASE");int mode=env?atoi(env):0;
 assert(mode>=0&&mode<=7);
 f->mode=mode;f->allowed=1;f->proof=1;
 if(mode==1)f->installed=1;
 struct mu_ap_ie_ops ops={f,ap_query,ap_write,ap_exclusive};
 int r=mu_ap_refresh_prepare_first_link(s,p,l,&ops,ap_bt,ap_id);
 if(mode==2||mode==3||mode==4){
  assert(r==CP_CLOUD_PROOF&&!f->writes&&!l->state&&!s->prepared);
  f->mode=0;f->ie_queries=0;
  r=mu_ap_refresh_prepare_first_link(s,p,l,&ops,ap_bt,ap_id);
 }
 assert(r==CP_CLOUD_OK&&s->prepared&&s->ap.channel==11);
 if(mode==5){f->proof=0;assert(mu_ap_refresh_commit(s,p,ap_verify,f)==CP_CLOUD_PROOF);f->proof=1;}
 if(mode==6){s->generation++;assert(mu_ap_refresh_commit(s,p,ap_verify,f)==CP_CLOUD_STALE);s->generation--;}
 if(mode==7){
  assert(mu_ap_refresh_commit(s,p,ap_verify,f)==CP_CLOUD_PROOF);
  assert(mu_ap_ie_restore(l,&ops)==0);
  f->mode=0;
  assert(mu_ap_refresh_prepare_first_link(s,p,l,&ops,ap_bt,ap_id)==CP_CLOUD_OK);
 }
 assert(mu_ap_refresh_commit(s,p,ap_verify,f)==CP_CLOUD_OK);
 assert(p->bootstrap->wireless.session==&s->session&&s->session.wifi.channel==11);
 assert(!strcmp(s->session.wifi.passphrase,"refreshed-fixture"));
 assert(mu_ap_refresh_clear(s,p)==CP_CLOUD_BAD);
 printf("R105149_AP_HANDOFF_CASE_PASS %d FIXTURE_ONLY\n",mode);
}
