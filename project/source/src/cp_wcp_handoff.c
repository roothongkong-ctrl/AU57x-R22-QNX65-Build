/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_wcp_handoff.h"
int cp_wcp_begin_cloud(struct cp_wcp_autoconnect *bt,
 struct cp_step04_session *session,struct cp_cloud_pipeline *cloud,
 const struct cp_cloud_owner *ops){
 if(!bt||!session||!cloud||!ops||!bt->generation||!session->epoch)
  return CP_WCP_INVALID;
 if(bt->state!=CP_WCP_READY||session->link.generation!=bt->generation||
    !bt->ops.stock_owner_verified||
    bt->ops.stock_owner_verified(bt->ops.ctx,bt->generation)!=1||
    !bt->ops.paired_acl_verified||
    bt->ops.paired_acl_verified(bt->ops.ctx,bt->generation)!=1||
    !bt->ops.wired_idle_verified||
    bt->ops.wired_idle_verified(bt->ops.ctx)!=1||
    !bt->ops.stock_iap2_connected||
    bt->ops.stock_iap2_connected(bt->ops.ctx,bt->generation)!=1)
  return CP_WCP_DENIED;
 /* Stock-authenticated control session 10, not an R87 peer assertion. */
 if(cp_s4_authenticated(session)!=1)return CP_WCP_WAIT;
 /* Existing R85 original SI/DIO pipeline retains USB priority, owner
  * checks, genuine 0x4301 ACK, AirPlay and media witnesses, and teardown. */
 return cp_cloud_begin(cloud,session,ops,session->epoch,bt->generation)
        ==CP_CLOUD_OK?CP_WCP_OK:CP_WCP_DENIED;
}
