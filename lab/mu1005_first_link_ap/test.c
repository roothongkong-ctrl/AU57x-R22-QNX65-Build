#include "ap_live.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* Host-only capture stub; production query is not exercised. */
int mhi2q_qnx_capture(const char*p,char*const a[],char*b,unsigned n,int*i){
 (void)p;(void)a;(void)b;(void)n;*i=1;return -1;
}
static const uint8_t bt[6]={0,17,34,51,68,85},id[6]={2,0,34,68,102,136};
static const char *ok="Index [2]\nManagement Subtype Mask = 0x12a\nIE Buffer: len=26\ndd 18 00 a0 40 00 00 02 80 22 06 06 00 11 22 33 44 55 07 06 02 00 22 44 66 88\n";
static const char *empty="Index [2]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\n";
struct f{int mode,writes,allowed,installed,queries;};
static int query(void*v,unsigned n,char*out,size_t cap){
 struct f*f=v;assert(n==6);f->queries++;
 const char *s="";char d[1024];
 switch(f->mode){
 case 0:s=f->installed?ok:empty;break;
 case 1:s=ok;break;
 case 2:s="";break;
 case 3:s="Index [2]\nManagement Subtype Mask = 0x12a\nIE Buffer: len=2\nff 00\n";break;
 case 4:if(f->queries>1)s=ok;else s=empty;break;
 case 5:s="Index [2]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\nIndex [2]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\n";break;
 default:return -1;}
 int z=snprintf(d,sizeof d,"Querying custom IE successful\n%s",s);assert(z>0&&(size_t)z<cap);memcpy(out,d,(size_t)z+1);return 0;
}
static int write_ie(void*v,unsigned s,const uint8_t*b,const uint8_t*i,int en){
 struct f*f=v;assert(s==2&&!memcmp(b,bt,6)&&!memcmp(i,id,6));
 f->writes++;f->installed=en;return 0;
}
static int owner(void*v,const uint8_t*b,unsigned s){return ((struct f*)v)->allowed&&b[0]==2&&s==2;}
int main(int argc,char**argv){
 assert(argc==2);
 int mode=atoi(argv[1]);struct f f={.mode=mode,.allowed=1};
 struct mu_ap_live ap={.ssid="fixture",.key="fixture-only",.bssid={2,0,0,0,0,1},.channel=11,.security=2};
 struct mu_ap_ie_ops o={&f,query,write_ie,owner};struct mu_ap_ie_lease l={0};unsigned slot=4;
 if(mode==0){
  assert(mu_ap_ie_select(&ap,&o,bt,id,&slot)==0&&slot==2);
  assert(mu_ap_ie_install_first_free(&l,&o,&ap,bt,id)==0&&l.state==1&&f.writes==1);
  assert(mu_ap_ie_restore(&l,&o)==0&&!l.state&&f.writes==2);
 }else if(mode==1){
  assert(mu_ap_ie_install_first_free(&l,&o,&ap,bt,id)==1&&!l.state&&!f.writes);
 }else{
  if(mode==6){f.mode=0;f.allowed=0;}
  if(mode==7){f.mode=0;ap.channel=36;}
  assert(mu_ap_ie_install_first_free(&l,&o,&ap,bt,id)<0&&!l.state&&!f.writes);
 }
 printf("FIRST_LINK_IE_CASE_PASS %d FIXTURE_ONLY\n",mode);return 0;
}
