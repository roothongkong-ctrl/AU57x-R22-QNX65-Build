/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_mu1005_bridge.h"
#include <string.h>
int cp_mu_bridge_init(struct cp_mu_bridge *b,AuRfIngress *rf,uint32_t epoch){
 if(!b||!rf||!epoch||rf->failed||rf->connected||rf->count||rf->tx_busy)
  return CP_MB_BAD;
 memset(b,0,sizeof *b);
 b->rf=rf;b->epoch=epoch;
 return CP_MB_OK;
}
int cp_mu_bridge_request_decided(struct cp_mu_bridge *b,uint32_t epoch,int accepted){
 if(!b||!b->rf||!epoch)return CP_MB_BAD;
 if(epoch!=b->epoch)return CP_MB_STALE;
 if(b->closed||!b->pending||b->accepted)return CP_MB_BLOCKED;
 /* Only the actual SDK owner may assert an accepted native event. A synthetic
  * return value in a host test is not proof of such an event on the car. */
 if(accepted!=1){b->closed=1;return CP_MB_OWNER;}
 b->accepted=1;return CP_MB_OK;
}
static int fail(struct cp_mu_bridge *b){
 b->closed=1;
 if(b->rf)au_rf_abort(b->rf);
 return CP_MB_BLOCKED;
}
int cp_mu_bridge_feed(struct cp_mu_bridge *b,const uint8_t snapshot[16],
                  uint32_t epoch,const struct cp_mu_bridge_owner *owner,
                  uint8_t *owned_rx,size_t cap){
 struct cp_mu_callback e;
 int rc;
 if(!b||!b->rf||!snapshot||!owner)return CP_MB_BAD;
 if(epoch!=b->epoch)return CP_MB_STALE;
 /* An OEM async TX completion may arrive AFTER native disconnect.
  * Preserve the borrowed descriptor until its real completion callback.
  * All other callbacks are rejected after closure. */
 if(b->closed && (snapshot[0]!=6 || !b->rf->tx_busy))
  return CP_MB_BLOCKED;
 rc=cp_mu_decode_snapshot(snapshot,16,b->epoch,epoch,
                           &owner->snapshot,&e,owned_rx,cap);
 if(rc==CP_MU_STALE)return CP_MB_STALE;
 if(rc!=CP_MU_OK)return fail(b);
 if(b->closed && e.code!=6)return CP_MB_BLOCKED;
 switch(e.code){
 case 1:
  if(b->pending||b->connected)return fail(b);
  memcpy(b->proposed_peer,e.peer_mac,6);
  b->pending=1;return CP_MB_PENDING;
 case 2:
  if(!b->pending||!b->accepted||b->connected||
     memcmp(b->proposed_peer,e.peer_mac,6))return fail(b);
  if(au_rf_connected(b->rf,b->proposed_peer)!=0)return fail(b);
  b->wire_generation=b->rf->generation;
  b->connected=1;b->pending=0;return CP_MB_OK;
 case 5:
  if(!b->connected||b->rf->generation!=b->wire_generation||
     au_rf_received(b->rf,owned_rx,e.length)!=0)return fail(b);
  return CP_MB_OK;
 case 6:
  if((!b->connected&&!b->closed)||!b->rf->tx_busy||!owner->tx_token_matches||
     owner->tx_token_matches(owner->snapshot.opaque,e.token,b->wire_generation)!=1||
     au_rf_send_complete(b->rf,e.status==0)!=0)return fail(b);
  return CP_MB_OK;
 case 4:
  if(!b->connected||au_rf_disconnected(b->rf)!=0)return fail(b);
  b->connected=0;b->closed=1;return CP_MB_OK;
 default:return fail(b);
 }
}
