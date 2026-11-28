/* SPDX-License-Identifier: GPL-3.0-only */
#include "wired_first.h"
#include <string.h>
static int nonzero(const MuPhoneId*p){
 unsigned i;for(i=0;i<sizeof p->bytes;i++)if(p->bytes[i])return 1;
 return 0;
}
int mu_wired_first_init(MuWiredFirst*s,MuPairedStart*p,const MuWiredFirstOps*o){
 if(!s||!p||!p->snapshot||!o||!o->usb_phone||!o->acl_phone||!o->load||!o->save)
  return CP_WCP_INVALID;
 memset(s,0,sizeof *s);s->paired=p;s->ops=*o;
 int r=o->load(o->ctx,&s->remembered);
 if(r==1&&nonzero(&s->remembered)){s->valid=1;return CP_WCP_OK;}
 return r==0?CP_WCP_OK:CP_WCP_DENIED;
}
int mu_wired_first_learn(MuWiredFirst*s,uint64_t cycle){
 MuPhoneId id={0},again={0};
 if(!s||!s->paired||!cycle)return CP_WCP_INVALID;
 if(cycle<=s->last_usb)return CP_WCP_STALE;
 /* USB must be active and authenticated by the original owner; only
  * store the stable paired identity, never the USB auth or session. */
 if(s->paired->bt.ops.wired_idle_verified(s->paired->ctx)!=0||
    s->ops.usb_phone(s->ops.ctx,cycle,&id)!=1||!nonzero(&id)||
    s->ops.usb_phone(s->ops.ctx,cycle,&again)!=1||
    memcmp(&id,&again,sizeof id))return CP_WCP_DENIED;
 if(!s->valid||memcmp(&id,&s->remembered,sizeof id)){
  if(s->ops.save(s->ops.ctx,&id)!=1)return CP_WCP_DENIED;
 }
 s->remembered=id;s->valid=1;s->last_usb=cycle;
 return CP_WCP_OK;
}
int mu_wired_first_tick(MuWiredFirst*s,uint64_t now){
 MuPhoneId id={0};uint32_t gen=0,again=0;
 if(!s||!s->paired)return CP_WCP_INVALID;
 MuPairedStart*p=s->paired;
 struct cp_wcp_autoconnect*b=&p->bt;
 /* Wired priority and pending cancel always run, even when no ID exists. */
 if(b->state==CP_WCP_RESTORE_PENDING)
  return cp_wcp_bt_restore_tick(b,b->generation);
 if(b->ops.wired_idle_verified(p->ctx)!=1)
  return mu_paired_start_tick(p,now);
 if(!s->valid||p->snapshot(p->ctx,&gen)!=1||!gen||
    s->ops.acl_phone(s->ops.ctx,gen,&id)!=1||
    memcmp(&id,&s->remembered,sizeof id)||
    p->snapshot(p->ctx,&again)!=1||gen!=again){
  if(b->state!=CP_WCP_IDLE&&b->state!=CP_WCP_BLOCKED)
   return cp_wcp_bt_abort(b,b->generation,1);
  return CP_WCP_WAIT;
 }
 /* Original paired ACL and original fresh wireless authentication.
  * Serialized OEM owner must ensure snapshot cannot change during tick. */
 return mu_paired_start_tick(p,now);
}
