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
 static const char*const cmd[]={"sys_cfg_bss_status","sys_cfg_ssid","sys_cfg_channel_ext","sys_cfg_ap_mac_address","sys_cfg_protocol","sys_cfg_wpa_passphrase"};
 int incomplete=0,n;int *uncertain=ctx;
 if(!uncertain||*uncertain||!out||cap<2||cap>INT_MAX||id>=6)return -1;
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
 }rc=0;
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
 if(!out||!ap||!receiver||ap->security!=2||!ap->channel||
    !memchr(ap->ssid,0,sizeof ap->ssid)||!memchr(ap->key,0,sizeof ap->key)||
    !ap->ssid[0]||strlen(ap->key)<8||receiver->airplay_port!=5000)return -1;
 s=*receiver;s.wifi.ssid=ap->ssid;s.wifi.passphrase=ap->key;s.wifi.security=ap->security;s.wifi.channel=ap->channel;s.wifi.bssid6=ap->bssid;
 if(cp_encode_wifi_5703(&s.wifi,frame,sizeof frame,&n)==CP_OK&&
    cp_encode_start_4301(&s,frame,sizeof frame,&n)==CP_OK){*out=s;rc=0;}
 mu_ap_live_clear(frame,sizeof frame);return rc;
}
