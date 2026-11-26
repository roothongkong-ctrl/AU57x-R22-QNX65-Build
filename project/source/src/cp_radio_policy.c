/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_radio_policy.h"
#include <string.h>
void cp_radio_init(struct cp_radio_policy *p) {
 if(p) memset(p,0,sizeof *p);
}
static int checked(const struct cp_radio_policy *p,uint32_t epoch,int owner) {
 if(!p||!epoch)return CP_RADIO_INVALID;
 if(epoch!=p->epoch)return CP_RADIO_STALE;
 if(owner!=1)return CP_RADIO_NO_OWNER;
 if(p->wired||p->owner_lost||p->phase==CP_RADIO_CLOSED)
  return CP_RADIO_BLOCKED;
 return CP_RADIO_OK;
}
enum cp_radio_band cp_radio_channel_band(unsigned channel){
 if(channel>=1&&channel<=14)return CP_RADIO_24;
 if(channel>=32&&channel<=177)return CP_RADIO_5;
 return CP_RADIO_NONE;
}
int cp_radio_begin(struct cp_radio_policy *p,uint32_t epoch,int cap24,
                   int cap5,int owner,int wired_idle) {
 return cp_radio_begin_preferred(p,epoch,cap24,cap5,owner,wired_idle,CP_RADIO_5);
}
int cp_radio_begin_preferred(struct cp_radio_policy *p,uint32_t epoch,int cap24,
                   int cap5,int owner,int wired_idle,enum cp_radio_band preferred) {
 if(!p||!epoch||cap24<0||cap24>1||cap5<0||cap5>1||
    owner!=1||wired_idle!=1||
    (preferred!=CP_RADIO_24&&preferred!=CP_RADIO_5))return CP_RADIO_INVALID;
 if(p->phase!=CP_RADIO_IDLE||p->epoch)return CP_RADIO_BAD_STATE;
 if(epoch<=p->last_epoch)return CP_RADIO_STALE;
 if(!cap24&&!cap5)return CP_RADIO_BLOCKED;
 p->epoch=p->last_epoch=epoch;
 p->cap24=(unsigned)cap24;p->cap5=(unsigned)cap5;p->preferred=preferred;
 p->tried24=p->tried5=0;p->selected=CP_RADIO_NONE;
 p->wired=p->owner_lost=0;p->phase=CP_RADIO_IDLE;
 return CP_RADIO_OK;
}
int cp_radio_select(struct cp_radio_policy *p,uint32_t epoch,
                    int owner,enum cp_radio_band *band) {
 int r=checked(p,epoch,owner);
 if(r!=CP_RADIO_OK)return r;
 if(!band)return CP_RADIO_INVALID;
 *band=CP_RADIO_NONE;
 if(p->phase!=CP_RADIO_IDLE)return CP_RADIO_BAD_STATE;
 /* Explicit first-connect 2.4 GHz preference when requested. The
  * legacy default remains 5 GHz. No fallback before physical teardown. */
 if(p->preferred==CP_RADIO_24&&p->cap24&&!p->tried24){
  p->selected=CP_RADIO_24;p->tried24=1;
 }else if(p->cap5&&!p->tried5){p->selected=CP_RADIO_5;p->tried5=1;}
 else if(p->cap24&&!p->tried24){p->selected=CP_RADIO_24;p->tried24=1;}
 else return CP_RADIO_NO_FALLBACK;
 p->phase=CP_RADIO_CHOSEN;
 *band=p->selected;
 return CP_RADIO_OK;
}
int cp_radio_connect_requested(struct cp_radio_policy *p,uint32_t epoch,int owner){
 int r=checked(p,epoch,owner);
 if(r!=CP_RADIO_OK)return r;
 if(p->phase!=CP_RADIO_CHOSEN)return CP_RADIO_BAD_STATE;
 p->phase=CP_RADIO_CONNECTING;
 return CP_RADIO_OK;
}
int cp_radio_association_result(struct cp_radio_policy *p,uint32_t epoch,
                                int owner,int associated,enum cp_radio_band observed,
                                int disconnect_confirmed){
 int r=checked(p,epoch,owner);
 if(r!=CP_RADIO_OK)return r;
 if(p->phase!=CP_RADIO_CONNECTING||!((associated==0)||(associated==1)))
  return CP_RADIO_BAD_STATE;
 if(observed!=p->selected)return CP_RADIO_BAD_STATE;
 if(associated==1){
  if(disconnect_confirmed!=0)return CP_RADIO_BAD_STATE;
  p->phase=CP_RADIO_ASSOCIATED;return CP_RADIO_OK;
 }
 if(disconnect_confirmed!=1)return CP_RADIO_PENDING;
 /* Retry once with a different positively verified band, never same band. */
 p->selected=CP_RADIO_NONE;p->phase=CP_RADIO_IDLE;
 if((p->cap5&&!p->tried5)||(p->cap24&&!p->tried24))
  return CP_RADIO_PENDING;
 p->phase=CP_RADIO_CLOSED;
 return CP_RADIO_NO_FALLBACK;
}
int cp_radio_revoke(struct cp_radio_policy *p,uint32_t epoch,int wired_started){
 if(!p||!epoch)return CP_RADIO_INVALID;
 if(epoch!=p->epoch)return CP_RADIO_STALE;
 if(wired_started!=0&&wired_started!=1)return CP_RADIO_INVALID;
 p->wired=(unsigned)wired_started;
 p->owner_lost=1;p->selected=CP_RADIO_NONE;p->phase=CP_RADIO_CLOSED;
 return CP_RADIO_OK;
}
int cp_radio_new_epoch_ready(struct cp_radio_policy *p,uint32_t epoch,
                             int actual_teardown_confirmed){
 if(!p||!epoch)return CP_RADIO_INVALID;
 if(epoch!=p->epoch)return CP_RADIO_STALE;
 if(p->phase!=CP_RADIO_CLOSED)return CP_RADIO_BAD_STATE;
 if(actual_teardown_confirmed!=1)return CP_RADIO_NO_OWNER;
 p->epoch=0;p->selected=CP_RADIO_NONE;p->phase=CP_RADIO_IDLE;
 p->tried24=p->tried5=p->cap24=p->cap5=0;
 p->wired=p->owner_lost=0;
 return CP_RADIO_OK;
}
