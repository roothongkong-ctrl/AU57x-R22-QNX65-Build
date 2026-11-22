/* Stock event -> R45 normalized RF -> R87 Bluetooth transport, NO OEM calls. */
#include "cp_wcp_r45_ingress.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct f {uint32_t gen;uint32_t rfgen;int owner,wired,published,paired,transport;uint8_t mac[6];};
static int own(void*p,uint32_t g){struct f*f=p;return f->owner&&g==f->gen;}
static int wired(void*p){return ((struct f*)p)->wired;}
static int published(void*p,uint32_t g){struct f*f=p;return f->published&&g==f->gen;}
static int paired(void*p,uint32_t g,const uint8_t m[6]){
 struct f*f=p;return f->paired&&g==f->gen&&memcmp(f->mac,m,6)==0;
}
static int transport(void*p,uint32_t g,uint32_t n){
 struct f*f=p;return f->transport&&g==f->gen&&n==f->rfgen;
}
int main(void){
 AuRfIngress rf;
 struct f f={0};
 struct cp_wcp_r45_ingress b={0};
 struct cp_wcp_bt_ops ops;
 struct cp_wcp_autoconnect bt;
 const uint8_t mac[6]={2,4,6,8,10,12};
 const uint8_t wrong[6]={1,3,5,7,9,11};
 f.gen=9;f.owner=f.wired=f.published=f.paired=f.transport=1;
 memcpy(f.mac,mac,6);
 struct cp_wcp_r45_owner orig={&f,own,wired,published,paired,transport};
 au_rf_init(&rf,0,0);
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==-1); /* no native RF event */
 assert(au_rf_connected(&rf,mac)==0);f.rfgen=rf.generation;
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==0);
 assert(cp_wcp_r45_ops(&b,&ops)==0);
 assert(ops.stock_open_iap2(ops.ctx,9)<0); /* cannot fake client open */
 assert(ops.phone_iap2_sdp(ops.ctx,9)<0);
 assert(cp_wcp_bt_init(&bt,&ops)==CP_WCP_OK);
 assert(cp_wcp_bt_acl_up(&bt,9,100)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&bt,9,101)==CP_WCP_LINK_READY); /* real R45 link */
 assert(bt.state==CP_WCP_READY);
 assert(cp_wcp_bt_abort(&bt,9,1)==CP_WCP_OK);
 assert(bt.state==CP_WCP_BLOCKED);
 assert(cp_wcp_bt_acl_up(&bt,9,200)==CP_WCP_DENIED);
 assert(au_rf_disconnected(&rf)==0);
 assert(ops.stock_iap2_connected(ops.ctx,9)==0); /* same MAC, NEW RF epoch */
 assert(cp_wcp_r45_ops(&b,&ops)==-1);
 cp_wcp_r45_unbind(&b);
 assert(au_rf_connected(&rf,wrong)==-1); /* previous queue not consumed */
 AuRfWire w;while(au_rf_take(&rf,&w)){};
 assert(au_rf_connected(&rf,wrong)==0);
 f.rfgen=rf.generation;
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==-1); /* reject nonpaired MAC */
 assert(au_rf_disconnected(&rf)==0);while(au_rf_take(&rf,&w)){};
 assert(au_rf_connected(&rf,mac)==0);f.rfgen=rf.generation;
 f.transport=0;
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==-1); /* no bridge */
 f.transport=1;f.wired=0;
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==-1); /* USB wins */
 f.wired=1;f.published=0;
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==-1); /* no actual SDP */
 f.published=1;
 assert(cp_wcp_r45_bind(&b,&rf,&orig,9)==0);
 assert(cp_wcp_r45_ops(&b,&ops)==0);
 assert(cp_wcp_bt_init(&bt,&ops)==CP_WCP_OK);
 assert(cp_wcp_bt_acl_up(&bt,9,1)==CP_WCP_OK);
 f.owner=0;
 assert(cp_wcp_bt_tick(&bt,9,2)==CP_WCP_DENIED);
 assert(bt.state==CP_WCP_BLOCKED);
 f.owner=1;
 cp_wcp_r45_unbind(&b);
 assert(ops.stock_iap2_connected(ops.ctx,9)==0);
 puts("R88 original R45 server-first to R87 radio: PASS");
 return 0;
}
