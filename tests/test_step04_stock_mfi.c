/* Synthetic export provider only, NEVER OEM car firmware or Apple data. */
#include "cp_step04_iap2_auth.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int owner_ok=1,stock_ok=1,control_ok=1,wired_idle=1,peer_ok=0;
static int cert_calls=0,sign_calls=0,releases=0,transmits=0,tx_ok=0;
static uint8_t sent[1400];static size_t sent_n;
static uint32_t sent_token;static unsigned sent_id;
int APSMFiPlatform_CopyCertificate(uint8_t **b,size_t *n){
 ++cert_calls;*b=malloc(12);if(!*b)return -1;
 for(int i=0;i<12;i++)(*b)[i]=(uint8_t)(0x30+i);
 *n=12;return 0;
}
int APSMFiPlatform_CreateSignature(const uint8_t *v,size_t size,uint8_t **b,size_t *n){
 ++sign_calls;if(size!=20)return -1;
 *b=malloc(20);if(!*b)return -1;
 for(int i=0;i<20;i++)(*b)[i]=(uint8_t)(v[i]^0x5a);
 *n=20;return 0;
}
static int has_owner(void *p,uint32_t e,uint32_t g){(void)p;return owner_ok&&e==7&&g==3;}
static int has_stock(void *p){(void)p;return stock_ok;}
static int has_session(void *p,uint32_t e,uint32_t g){(void)p;return control_ok&&e==7&&g==3;}
static int no_wired(void *p){(void)p;return wired_idle;}
static void release(void *p,void *q){(void)p;++releases;free(q);}
static int send_frame(void *p,uint32_t e,uint32_t g,const uint8_t *b,size_t n,uint32_t token){
 (void)p;assert(e==7&&g==3&&n<=sizeof sent);++transmits;
 memcpy(sent,b,n);sent_n=n;sent_token=token;sent_id=((unsigned)b[4]<<8)|b[5];
 return 0;
}
static int tx_confirmed(void *p,uint32_t e,uint32_t g,uint32_t token,uint16_t id){
 (void)p;return e==7&&g==3&&token==sent_token&&id==sent_id&&tx_ok;
}
static int peer_confirmed(void *p,uint32_t e,uint32_t g){
 (void)p;return e==7&&g==3&&peer_ok;
}
static int wireless_tx(void *p,const uint8_t *b,size_t n){
 (void)p;assert(n>=6&&b[4]==0x57&&b[5]==3);return 0;
}
static void msg(uint8_t *b,unsigned id){
 b[0]=0x40;b[1]=0x40;b[2]=0;b[3]=6;b[4]=(uint8_t)(id>>8);b[5]=(uint8_t)id;
}
int main(void){
 const struct cp_stock_owner o={0,has_owner,has_stock,has_session,no_wired,release};
 const struct cp_auth_owner a={0,has_session,no_wired,send_frame,tx_confirmed,peer_confirmed};
 struct cp_stock_mfi stock;struct cp_step04_auth auth;
 uint8_t buf[1280];size_t n=0;uint8_t x[40]={0};
 void *lib=dlopen(NULL,RTLD_NOW);assert(lib);
 owner_ok=0;
 assert(cp_stock_mfi_bind(&stock,lib,&o,7,3)==CP_STOCK_OWNER);
 owner_ok=1;stock_ok=0;
 assert(cp_stock_mfi_bind(&stock,lib,&o,7,3)==CP_STOCK_OWNER);
 stock_ok=1;
 assert(cp_stock_mfi_bind(&stock,lib,&o,7,3)==CP_STOCK_OK);
 assert(cp_stock_mfi_cert(&stock,buf,sizeof buf,&n)==CP_STOCK_OK&&n==12);
 assert(releases==1);
 assert(cp_stock_mfi_sign(&stock,(const uint8_t*)"short",5,buf,sizeof buf,&n)==CP_STOCK_INVALID);
 assert(sign_calls==0);
 assert(cp_auth_init(&auth,&stock,&a,7,3)==CP_AUTH_OK);
 msg(x,0xAA05);
 assert(cp_auth_receive(&auth,x,6)==CP_AUTH_OWNER);
 assert(auth.stage==CP_AUTH_FAILED&&!cp_auth_live(&auth));
 assert(cp_auth_init(&auth,&stock,&a,7,3)==CP_AUTH_OK);
 msg(x,0xAA00);
 assert(cp_auth_receive(&auth,x,6)==CP_AUTH_TX_PENDING);
 assert(sent_n==22&&sent_id==0xAA01&&sent[7]==16);
 uint32_t cert_token=sent_token;
 assert(cp_auth_tx_confirm(&auth,cert_token+1)==CP_AUTH_ORDER);
 assert(!cp_auth_live(&auth));
 tx_ok=1;assert(cp_auth_tx_confirm(&auth,cert_token)==CP_AUTH_OK);
 msg(x,0xAA02);x[3]=30;x[7]=24;
 for(int i=0;i<20;i++)x[10+i]=(uint8_t)i;
 assert(cp_auth_receive(&auth,x,29)==CP_AUTH_BAD&&auth.stage==CP_AUTH_FAILED);
 assert(cp_auth_init(&auth,&stock,&a,7,3)==CP_AUTH_OK);
 msg(x,0xAA00);
 assert(cp_auth_receive(&auth,x,6)==CP_AUTH_TX_PENDING);
 assert(cp_auth_tx_confirm(&auth,sent_token)==CP_AUTH_OK);
 msg(x,0xAA02);x[3]=30;x[7]=24;
 for(int i=0;i<20;i++)x[10+i]=(uint8_t)i;
 assert(cp_auth_receive(&auth,x,30)==CP_AUTH_TX_PENDING);
 assert(sent_n==30&&sent_id==0xAA03&&auth.stage==CP_AUTH_SIGNATURE_PENDING);
 for(int i=0;i<20;i++)assert(sent[10+i]==(uint8_t)(i^0x5a));
 assert(cp_auth_tx_confirm(&auth,sent_token)==CP_AUTH_OK);
 assert(auth.stage==CP_AUTH_AWAIT_PEER&&!cp_auth_live(&auth));
 peer_ok=1;msg(x,0xAA05);
 assert(cp_auth_receive(&auth,x,6)==CP_AUTH_OK&&cp_auth_live(&auth));
 struct cp_wireless wl;struct cp_session sess={0};
 sess.wifi.ssid="TEST";sess.wifi.passphrase="fake-pass";
 sess.wifi.security=2;sess.wifi.channel=6;
 cp_wireless_init(&wl,&sess,wireless_tx,NULL);
 struct cp_oem_assertions p={0};
 p.verified_control_session=1;p.live_hotspot=1;p.sole_dio_owner=1;
 uint8_t req[]={0x40,0x40,0,6,0x57,0x02};
 assert(cp_step04_dispatch(&auth,&wl,&p,req,sizeof req)==CP_WL_OK);
 assert(wl.wifi_pre_sent==1);
 wired_idle=0;assert(!cp_auth_live(&auth));
 wired_idle=1;assert(!cp_auth_live(&auth)); /* cannot revive stale auth */
 assert(cp_step04_dispatch(&auth,&wl,&p,req,sizeof req)==CP_WL_NOT_READY);
 assert(sign_calls==1&&releases>=4&&transmits==3);
 cp_stock_mfi_invalidate(&stock);dlclose(lib);
 puts("AU57X_R41_STOCK_MFI_IAP2_CSM_HOST_PASS");return 0;
}
