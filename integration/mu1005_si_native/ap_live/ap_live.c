#include "ap_live.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
extern int mhi2q_qnx_capture(const char*,char*const[],char*,unsigned,int*);
void mu_ap_live_clear(void*p,size_t n){volatile unsigned char*v=p;while(n--)*v++=0;}
/* Use original command path, explicit uap0, no shell and no setter. A timed
 * out driver ioctl is not known to be cancelled. Caller stops on failure. */
int mu_ap_stock_query(void*ctx,unsigned id,char*out,size_t cap){
 static const char*const cmd[]={"sys_cfg_bss_status","sys_cfg_ssid","sys_cfg_channel_ext","sys_cfg_ap_mac_address","sys_cfg_protocol","sys_cfg_wpa_passphrase","sys_cfg_custom_ie"};
 int incomplete=0,n;int *uncertain=ctx;
 if(!uncertain||*uncertain||!out||cap<2||cap>INT_MAX||id>=7)return -1;
 char *argv[]={"/mnt/app/armle/sbin/uaputl","-i","uap0",(char*)cmd[id],NULL};
 n=mhi2q_qnx_capture(argv[0],argv,out,(unsigned)cap,&incomplete);
 if(incomplete)*uncertain=1;
 if(incomplete||n<=0||(size_t)n>=cap||memchr(out,0,(size_t)n)){mu_ap_live_clear(out,cap);return -1;}
 out[n]=0;return 0;
}
static int field(const char*b,const char*prefix,char*out,size_t cap){
 size_t p=strlen(prefix);int found=0;
 while(*b){const char*e=strchr(b,'\n');size_t n=e?(size_t)(e-b):strlen(b);
  if(n&&b[n-1]=='\r')--n;
  if(n>=p&&!memcmp(b,prefix,p)){size_t k=n-p;if(found||!k||k>=cap)return -1;memcpy(out,b+p,k);out[k]=0;found=1;}
  if(!e)break;
  b=e+1;
 }return found?0:-1;
}
static int hex(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int mac(const char*s,uint8_t*b){unsigned any=0;if(strlen(s)!=17)return -1;
 for(unsigned i=0;i<6;i++){int a=hex(s[i*3]),c=hex(s[i*3+1]);if(a<0||c<0||(i<5&&s[i*3+2]!=':'))return -1;b[i]=(uint8_t)(16*a+c);any|=b[i];}return any&&!(b[0]&1)?0:-1;}
/* Validate both driver-parsed snapshots and direct caller-supplied snapshots.
 * These are wire-format bounds, not proof of regional channel availability. */
static int ap_valid(const struct mu_ap_live *s){
 size_t n;unsigned any=0;
 if(!s||s->security!=2||!memchr(s->ssid,0,sizeof s->ssid)||
    !memchr(s->key,0,sizeof s->key)||!s->ssid[0]||
    !((s->channel>=1&&s->channel<=14)||(s->channel>=32&&s->channel<=177)))return 0;
 for(unsigned i=0;i<6;i++)any|=s->bssid[i];
 if(!any||(s->bssid[0]&1))return 0;
 n=strlen(s->key);if(n<8||n>64)return 0;
 for(size_t i=0;i<n;i++){
  unsigned char k=(unsigned char)s->key[i];
  if(n==64?hex(k)<0:k<32||k>126)return 0;
 }
 return 1;
}
static int sample(struct mu_ap_live*s,mu_ap_query q,void*ctx){
 char buf[1024]={0},v[128]={0},band[16]={0},*end;long ch;int rc=-1;
 for(unsigned i=0;i<6;i++){
  mu_ap_live_clear(buf,sizeof buf);mu_ap_live_clear(v,sizeof v);
  if(q(ctx,i,buf,sizeof buf)||!memchr(buf,0,sizeof buf)||strstr(buf,"ERR:"))goto done;
  switch(i){
  case 0:if(field(buf,"BSS status = ",v,sizeof v)||strcmp(v,"started"))goto done;break;
  case 1:if(field(buf,"SSID = ",s->ssid,sizeof s->ssid))goto done;break;
  case 2:if(field(buf,"Channel = ",v,sizeof v)||field(buf,"Band    = ",band,sizeof band))goto done;
   ch=strtol(v,&end,10);if(*end||!((ch>=1&&ch<=14&&!strcmp(band,"2.4GHz"))||(ch>=32&&ch<=177&&!strcmp(band,"5GHz"))))goto done;
   s->channel=(uint8_t)ch;break;
  case 3:if(field(buf,"AP MAC address = ",v,sizeof v)||mac(v,s->bssid))goto done;break;
  case 4:if(field(buf,"PROTOCOL = ",v,sizeof v)||(strcmp(v,"WPA2")&&strcmp(v,"WPA2 ")))goto done;s->security=2;break;
  case 5:if(field(buf,"WPA passphrase = ",s->key,sizeof s->key))goto done;
   {size_t n=strlen(s->key);if(n<8||n>64)goto done;if(n==64)for(size_t j=0;j<n;j++)if(hex(s->key[j])<0)goto done;}break;
  }
 }rc=ap_valid(s)?0:-1;
done:mu_ap_live_clear(buf,sizeof buf);mu_ap_live_clear(v,sizeof v);return rc;
}
int mu_ap_live_read(struct mu_ap_live*out,mu_ap_query q,void*ctx){
 struct mu_ap_live a={0},b={0};int rc=-1;
 if(!out||!q)return -1;
 mu_ap_live_clear(out,sizeof *out);
 if(!sample(&a,q,ctx)&&!sample(&b,q,ctx)&&!memcmp(&a,&b,sizeof a)){*out=a;rc=0;}
 mu_ap_live_clear(&a,sizeof a);mu_ap_live_clear(&b,sizeof b);return rc;
}
int mu_ap_live_session(struct cp_session*out,const struct mu_ap_live*ap,const struct cp_session*receiver){
 unsigned char frame[4096];size_t n=0;struct cp_session s;int rc=-1;
 if(!out||!receiver||!ap_valid(ap)||receiver->airplay_port!=5000)return -1;
 s=*receiver;s.wifi.ssid=ap->ssid;s.wifi.passphrase=ap->key;s.wifi.security=ap->security;s.wifi.channel=ap->channel;s.wifi.bssid6=ap->bssid;
 if(cp_encode_wifi_5703(&s.wifi,frame,sizeof frame,&n)==CP_OK&&
    cp_encode_start_4301(&s,frame,sizeof frame,&n)==CP_OK){*out=s;rc=0;}
 mu_ap_live_clear(frame,sizeof frame);return rc;
}

/* Read-only adaptation of the GPL-3.0 upstream LuKa wifi_ie.rs
 * (20c94f51c0f0ae88b154c12e4f604643ba23022c).
 * Only the upstream-confirmed 2.4 GHz Device IE is recognized. Neither
 * this query nor a matching IE proves association or MFi authorization. */
static int ie_identity(const uint8_t *a){
 unsigned any=0;if(!a||(a[0]&1))return 0;
 for(unsigned i=0;i<6;i++)any|=a[i];
 return any!=0;
}
int mu_ap_device_ie_verify(const char *out,unsigned slot,const uint8_t bt[6],const uint8_t id[6]){
 uint8_t expected[26]={0xdd,0x18,0,0xa0,0x40,0,0,2,0x80,0x22,6,6,0,0,0,0,0,0,7,6};
 char label[16];const char *p,*e;unsigned found=0;
 if(!out||slot>3||!ie_identity(bt)||!ie_identity(id))return -1;
 if(!strstr(out,"Querying custom IE successful")||strstr(out,"ERR:"))return -1;
 memcpy(expected+12,bt,6);memcpy(expected+20,id,6);
 /* Slot names are bounded by upstream utility's documented range. */
 memcpy(label,"Index [0]",10);label[7]=(char)('0'+slot);
 p=out;
 while(*p){
  e=strchr(p,'\n');size_t n=e?(size_t)(e-p):strlen(p);
  if(n&&p[n-1]=='\r')--n;
  if(n==9&&!memcmp(p,label,9)){if(found++)return -1;}
  if(!e)break;
  p=e+1;
 }
 if(!found)return 0;
 p=strstr(out,label);if(!p)return -1;
 p=strchr(p,'\n');if(!p)return -1;++p;
 const char *mask="Management Subtype Mask = 0x";size_t ml=strlen(mask);
 if(strncmp(p,mask,ml))return -1;
 p+=ml;unsigned m=0,digits=0;
 while(hex((unsigned char)*p)>=0){if(++digits>4)return -1;m=m*16u+(unsigned)hex((unsigned char)*p++);}
 if(!digits)return -1;
 if(*p=='\r')++p;
 if(*p++!='\n')return -1;
 const char *len="IE Buffer: len=";size_t ll=strlen(len);
 if(strncmp(p,len,ll))return -1;
 p+=ll;unsigned z=0;digits=0;
 while(*p>='0'&&*p<='9'){if(++digits>3)return -1;z=z*10u+(unsigned)(*p++-'0');}
 if(!digits||z>256)return -1;
 if(*p=='\r')++p;
 if(*p++!='\n')return -1;
 if(!z)return 0;
 if(z!=sizeof expected||m!=0x12au)return -1;
 for(unsigned i=0;i<z;i++){
  while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p;
  int a=hex((unsigned char)*p);if(a<0)return -1;++p;
  int b=hex((unsigned char)*p);if(b<0)return -1;++p;
  if((uint8_t)(16*a+b)!=expected[i])return -1;
  if(*p&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n')return -1;
 }
 /* Do not silently accept a trailing extra byte in the same hexdump. */
 while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p;
 if(*p&&strncmp(p,"Index [",7))return -1;
 return 1;
}
int mu_ap_live_verify_device_ie(const struct mu_ap_live *ap,mu_ap_query q,void *ctx,unsigned slot,const uint8_t bt[6],const uint8_t id[6]){
 char buf[4096]={0};int r=-1;
 if(!ap_valid(ap)||ap->channel>14||!q||slot>3)return -1;
 if(!q(ctx,6,buf,sizeof buf)&&memchr(buf,0,sizeof buf))r=mu_ap_device_ie_verify(buf,slot,bt,id);
 mu_ap_live_clear(buf,sizeof buf);return r;
}
