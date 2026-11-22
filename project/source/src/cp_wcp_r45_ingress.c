/* SPDX-License-Identifier: GPL-3.0-only */
#include "cp_wcp_r45_ingress.h"
#include <string.h>
static int mac_valid(const uint8_t x[6]){
 unsigned any=0;size_t i;
 for(i=0;i<6;i++)any|=x[i];
 return any!=0;
}
/* No owner/provider state can be inferred merely from this heap struct.
 * Require the real original btstack process owner to independently attest
 * the current generation, paired device, published RFCOMM service and bridge. */
static int live(const struct cp_wcp_r45_ingress *b,uint32_t gen){
 if(!b||!b->bound||!gen||gen!=b->generation||!b->rf||!b->rf_generation||
    b->rf->failed||!b->rf->connected||
    b->rf->generation!=b->rf_generation||
    memcmp(b->rf->peer,b->peer,6)!=0||!mac_valid(b->peer)||
    !b->o.owner_verified||!b->o.original_rf_server_published||
    !b->o.paired_acl_authorized||!b->o.r45_transport_live||
    b->o.owner_verified(b->o.ctx,gen)!=1||
    b->o.original_rf_server_published(b->o.ctx,gen)!=1||
    b->o.paired_acl_authorized(b->o.ctx,gen,b->peer)!=1||
    b->o.r45_transport_live(b->o.ctx,gen,b->rf_generation)!=1)return 0;
 return 1;
}
int cp_wcp_r45_bind(struct cp_wcp_r45_ingress *b,const AuRfIngress *rf,
 const struct cp_wcp_r45_owner *o,uint32_t gen){
 if(!b||!rf||!o||!gen||!o->owner_verified||!o->wired_idle||
    !o->original_rf_server_published||!o->paired_acl_authorized||
    !o->r45_transport_live||!rf->connected||rf->failed||
    !rf->generation||!mac_valid(rf->peer)||b->bound)return -1;
 /* Use staged assignment so failed binding never leaves a usable route. */
 memset(b,0,sizeof *b);b->rf=rf;b->o=*o;
 b->generation=gen;b->rf_generation=rf->generation;
 memcpy(b->peer,rf->peer,6);b->bound=1;
 if(!live(b,gen)||o->wired_idle(o->ctx)!=1){
  memset(b,0,sizeof *b);return -1;
 }
 return 0;
}
static int w_owner(void *p,uint32_t gen){return live((struct cp_wcp_r45_ingress *)p,gen);}
static int w_acl(void *p,uint32_t gen){return live((struct cp_wcp_r45_ingress *)p,gen);}
static int w_wired(void *p){
 struct cp_wcp_r45_ingress *b=p;
 return b&&b->bound&&b->o.wired_idle&&b->o.wired_idle(b->o.ctx)==1;
}
static int w_link(void *p,uint32_t gen){return live((struct cp_wcp_r45_ingress *)p,gen);}
/* Server-first route: a client SDP result is UNKNOWN, never 'present'. */
static int w_sdp(void *p,uint32_t gen){(void)p;(void)gen;return -1;}
/* Do not launch client RFCOMM against an unproven MU1005 BlueSDK ABI. */
static int w_open(void *p,uint32_t gen){(void)p;(void)gen;return -1;}
/* This route has no outstanding client-open. Refuse fabricated cancel ACK. */
static int w_cancel(void *p,uint32_t gen){(void)p;(void)gen;return 0;}
int cp_wcp_r45_ops(struct cp_wcp_r45_ingress *b,struct cp_wcp_bt_ops *out){
 if(!b||!out||!live(b,b->generation))return -1;
 memset(out,0,sizeof *out);out->ctx=b;
 out->stock_owner_verified=w_owner;
 out->paired_acl_verified=w_acl;
 out->wired_idle_verified=w_wired;
 out->stock_iap2_connected=w_link;
 out->phone_iap2_sdp=w_sdp;
 out->stock_open_iap2=w_open;
 out->stock_cancel_iap2=w_cancel;
 return 0;
}
void cp_wcp_r45_unbind(struct cp_wcp_r45_ingress *b){if(b)memset(b,0,sizeof *b);}
