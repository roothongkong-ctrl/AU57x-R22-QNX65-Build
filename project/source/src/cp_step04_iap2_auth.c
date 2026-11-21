/* SPDX-License-Identifier: GPL-3.0-only
 * AA00/01 certificate, AA02/03 20-byte challenge, AA04/05 peer status.
 * Reference: user-provided GPL-3.0 R14 iap2-csm/control machine.
 */
#include "cp_step04_iap2_auth.h"
#include <string.h>
#define CSM_LIMIT 4096u
#define AA00 0xAA00u
#define AA01 0xAA01u
#define AA02 0xAA02u
#define AA03 0xAA03u
#define AA04 0xAA04u
#define AA05 0xAA05u
static unsigned be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void wipe(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static int authentic(struct cp_step04_auth *a){
 return a&&a->stock&&a->stage>CP_AUTH_IDLE&&a->stage<CP_AUTH_FAILED&&
  a->owner.control_live&&a->owner.wired_idle&&
  a->owner.control_live(a->owner.opaque,a->epoch,a->bt_generation)==1&&
  a->owner.wired_idle(a->owner.opaque)==1&&cp_stock_mfi_live(a->stock)==1&&
  a->stock->epoch==a->epoch&&a->stock->bt_generation==a->bt_generation;
}
static int fail(struct cp_step04_auth *a,int status){
 if(a){a->stage=CP_AUTH_FAILED;a->tx_id=0;a->tx_token=0;}
 return status;
}
int cp_auth_init(struct cp_step04_auth *a,struct cp_stock_mfi *stock,
 const struct cp_auth_owner *o,uint32_t epoch,uint32_t gen){
 if(!a||!stock||!o||!epoch||!gen||!o->control_live||!o->wired_idle||
    !o->send_csm||!o->tx_committed||!o->peer_auth_succeeded)return CP_AUTH_BAD;
 memset(a,0,sizeof *a);
 a->stock=stock;a->owner=*o;a->epoch=epoch;a->bt_generation=gen;
 a->stage=CP_AUTH_AWAIT_CERT;
 if(!authentic(a)){a->stage=CP_AUTH_FAILED;return CP_AUTH_OWNER;}
 return CP_AUTH_OK;
}
static int submit(struct cp_step04_auth *a,uint16_t id,const uint8_t *data,
                  size_t n,enum cp_auth_stage next){
 uint8_t frame[CSM_LIMIT];size_t total=n+10u;
 uint32_t token;
 if(n>CSM_LIMIT-10||total>65535)return fail(a,CP_AUTH_BAD);
 if(a->sequence==UINT32_MAX)return fail(a,CP_AUTH_BAD);
 token=++a->sequence;
 put16(frame,0x4040);put16(frame+2,(unsigned)total);
 put16(frame+4,id);put16(frame+6,(unsigned)(n+4));put16(frame+8,0);
 memcpy(frame+10,data,n);
 if(a->owner.send_csm(a->owner.opaque,a->epoch,a->bt_generation,
                       frame,total,token)!=0){
  wipe(frame,total);return fail(a,CP_AUTH_OEM);
 }
 wipe(frame,total);
 a->tx_token=token;a->tx_id=id;a->stage=next;
 return CP_AUTH_TX_PENDING;
}
int cp_auth_receive(struct cp_step04_auth *a,const uint8_t *frame,size_t len){
 unsigned id;uint8_t data[CP_STOCK_CERT_MAX];uint8_t challenge[CP_STOCK_CHALLENGE];
 size_t n=0;int r;
 if(!a||!frame)return CP_AUTH_BAD;
 if(a->stage==CP_AUTH_FAILED||a->stage==CP_AUTH_IDLE)return CP_AUTH_BLOCKED;
 if(len<6||len>CSM_LIMIT)return fail(a,CP_AUTH_BAD);
 if(!authentic(a))return fail(a,CP_AUTH_OWNER);
 if(be16(frame)!=0x4040||be16(frame+2)!=len)return fail(a,CP_AUTH_BAD);
 id=be16(frame+4);
 if(id==AA04)return fail(a,CP_AUTH_OEM);
 if(id==AA00){
  if(a->stage!=CP_AUTH_AWAIT_CERT||len!=6)return fail(a,CP_AUTH_ORDER);
  r=cp_stock_mfi_cert(a->stock,data,sizeof data,&n);
  if(r!=CP_STOCK_OK)return fail(a,CP_AUTH_OEM);
  r=submit(a,AA01,data,n,CP_AUTH_CERT_PENDING);
  wipe(data,sizeof data);return r;
 }
 if(id==AA02){
  if(a->stage!=CP_AUTH_AWAIT_CHALLENGE||len!=30||
     be16(frame+6)!=24||be16(frame+8)!=0)return fail(a,CP_AUTH_ORDER);
  memcpy(challenge,frame+10,sizeof challenge);
  r=cp_stock_mfi_sign(a->stock,challenge,sizeof challenge,data,sizeof data,&n);
  wipe(challenge,sizeof challenge);
  if(r!=CP_STOCK_OK)return fail(a,CP_AUTH_OEM);
  r=submit(a,AA03,data,n,CP_AUTH_SIGNATURE_PENDING);
  wipe(data,sizeof data);return r;
 }
 if(id==AA05){
  if(a->stage!=CP_AUTH_AWAIT_PEER||len!=6||
     a->owner.peer_auth_succeeded(a->owner.opaque,a->epoch,a->bt_generation)!=1)
      return fail(a,CP_AUTH_OWNER);
  a->stage=CP_AUTH_SUCCEEDED;return CP_AUTH_OK;
 }
 return fail(a,CP_AUTH_BAD);
}
int cp_auth_tx_confirm(struct cp_step04_auth *a,uint32_t token){
 if(!a||!token||token!=a->tx_token)return CP_AUTH_ORDER;
 if(a->stage!=CP_AUTH_CERT_PENDING&&a->stage!=CP_AUTH_SIGNATURE_PENDING)
  return CP_AUTH_ORDER;
 if(!authentic(a))return fail(a,CP_AUTH_OWNER);
 if(a->owner.tx_committed(a->owner.opaque,a->epoch,a->bt_generation,
                          token,a->tx_id)!=1)return fail(a,CP_AUTH_OWNER);
 a->tx_token=0;a->tx_id=0;
 a->stage=a->stage==CP_AUTH_CERT_PENDING?
  CP_AUTH_AWAIT_CHALLENGE:CP_AUTH_AWAIT_PEER;
 return CP_AUTH_OK;
}
int cp_auth_live(struct cp_step04_auth *a){
 if(!a||a->stage!=CP_AUTH_SUCCEEDED)return 0;
 if(!authentic(a)||
    a->owner.peer_auth_succeeded(a->owner.opaque,a->epoch,a->bt_generation)!=1){
  (void)fail(a,CP_AUTH_OWNER);return 0;
 }
 return 1;
}
int cp_auth_revoke(struct cp_step04_auth *a){
 if(!a)return CP_AUTH_BAD;
 a->stage=CP_AUTH_FAILED;a->tx_id=0;a->tx_token=0;
 return CP_AUTH_OK;
}
int cp_step04_dispatch(struct cp_step04_auth *a,struct cp_wireless *w,
 const struct cp_oem_assertions *other,const uint8_t *csm,size_t n){
 struct cp_oem_assertions p;
 if(!a||!w||!other||!csm||!n)return CP_WL_BAD_FRAME;
 if(!cp_auth_live(a))return CP_WL_NOT_READY;
 p=*other;p.verified_mfi=1;
 return cp_oem_dispatch(w,&p,csm,n);
}
