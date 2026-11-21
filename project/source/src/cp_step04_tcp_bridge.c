/* SPDX-License-Identifier: GPL-3.0-only
 * Maps exact R14 wire format {type,reserved,lenLE16,generationLE32,payload}
 * into R44 complete in-memory AuRfWire and back, no local server or real owner.
 */
#include "cp_step04_tcp_bridge.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
        ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static size_t le16(const uint8_t *p) {
 return (size_t)p[0]|((size_t)p[1]<<8);
}
static int authorized(const struct cp_s4_tcp_bridge *t) {
 return t&&t->session&&t->epoch&&t->session->epoch==t->epoch&&
 t->peer_authorized&&t->peer_authorized(t->opaque,t->epoch)==1&&
 !t->closed;
}
void cp_s4_tcp_eof(struct cp_s4_tcp_bridge *t) {
 if(!t)return;
 t->closed=1;t->connected=0;
 t->rx_used=t->rx_expected=t->tx_size=t->tx_sent=0;
 memset(t->tx,0,sizeof t->tx);
 if(t->session)cp_s4_revoke(t->session);
}
int cp_s4_tcp_init(struct cp_s4_tcp_bridge *t,struct cp_step04_session *s,
                   int (*peer_authorized)(void *,uint32_t),void *opaque) {
 if(!t||!s||!s->epoch||!peer_authorized||s->link.phase!=CP_LK_IDLE)return CP_LK_BAD;
 memset(t,0,sizeof *t);t->session=s;t->epoch=s->epoch;
 t->peer_authorized=peer_authorized;t->opaque=opaque;
 if(!authorized(t)){cp_s4_tcp_eof(t);return CP_LK_OWNER;}
 return CP_LK_OK;
}
static int dispatch(struct cp_s4_tcp_bridge *t,const uint8_t *pkt,size_t len,
                    uint64_t now) {
 uint8_t kind=pkt[0];size_t n=le16(pkt+2);uint32_t gen=le32(pkt+4);
 AuRfWire frame;
 int rc;
 if(pkt[1]||!gen||n+8!=len)return CP_LK_BAD;
 if(kind==1) {
  if(t->connected||t->generation||n!=6||t->session->link.phase!=CP_LK_IDLE)
   return CP_LK_BAD;
 } else if(kind==2){
  if(!t->connected||gen!=t->generation||n==0||n>AU_RF_MAX_DATA)
   return CP_LK_STALE;
 } else if(kind==3){
  if(!t->connected||gen!=t->generation||n)return CP_LK_STALE;
 } else return CP_LK_BAD;
 if(!authorized(t))return CP_LK_OWNER;
 memcpy(frame.bytes,pkt,len);frame.len=len;
 rc=cp_s4_accept_rf(t->session,&frame,now);
 if(rc==CP_LK_OK){
  if(kind==1){t->connected=1;t->generation=gen;}
  else if(kind==3){t->connected=0;t->closed=1;}
 }
 return rc;
}
int cp_s4_tcp_feed(struct cp_s4_tcp_bridge *t,const uint8_t *data,size_t size,
                   uint64_t now){
 if(!t||!data||!size)return CP_LK_BAD;
 if(!authorized(t)) {cp_s4_tcp_eof(t);return CP_LK_OWNER;}
 while(size){
  size_t expected=t->rx_expected?t->rx_expected:8u;
  if(t->rx_used>=expected){cp_s4_tcp_eof(t);return CP_LK_BAD;}
  size_t take=expected-t->rx_used;
  if(take>size)take=size;
  memcpy(t->rx+t->rx_used,data,take);
  t->rx_used+=take;data+=take;size-=take;
  if(t->rx_used==8u&&!t->rx_expected){
   size_t n=le16(t->rx+2);
   if(t->rx[1]||n>AU_RF_MAX_DATA||
      (t->rx[0]==1&&n!=6)||(t->rx[0]==2&&!n)||
      (t->rx[0]==3&&n)||(t->rx[0]!=1&&t->rx[0]!=2&&t->rx[0]!=3)){
    cp_s4_tcp_eof(t);return CP_LK_BAD;
   }
   t->rx_expected=8u+n;
  }
  if(t->rx_expected&&t->rx_used==t->rx_expected){
   int rc=dispatch(t,t->rx,t->rx_used,now);
   t->rx_used=t->rx_expected=0;
   if(rc!=CP_LK_OK){
    cp_s4_tcp_eof(t);return rc;
   }
   if(t->closed)return CP_LK_OK;
  }
 }
 return CP_LK_OK;
}
int cp_s4_tcp_peek(struct cp_s4_tcp_bridge *t,const uint8_t **data,size_t *len){
 if(!t||!data||!len)return CP_LK_BAD;
 if(!authorized(t)||!t->connected){cp_s4_tcp_eof(t);return CP_LK_OWNER;}
 if(!t->tx_size){
  const uint8_t *raw=NULL;size_t raw_len=0;
  int n=cp_lk_peek(&t->session->link,&raw,&raw_len);
  if(n<=0)return n;
  if(!raw||!raw_len||raw_len>CP_LK_FRAME_MAX||!t->generation){
   cp_s4_tcp_eof(t);return CP_LK_BAD;
  }
  t->tx[0]=2;t->tx[1]=0;
  t->tx[2]=(uint8_t)raw_len;t->tx[3]=(uint8_t)(raw_len>>8);
  for(unsigned i=0;i<4;i++)t->tx[4+i]=(uint8_t)(t->generation>>(i*8));
  memcpy(t->tx+8,raw,raw_len);
  t->tx_size=8u+raw_len;t->tx_sent=0;
 }
 *data=t->tx+t->tx_sent;*len=t->tx_size-t->tx_sent;
 return 1;
}
int cp_s4_tcp_commit(struct cp_s4_tcp_bridge *t,size_t n){
 if(!t||!authorized(t)||!t->connected||!t->tx_size)return CP_LK_BAD;
 if(!n||n>t->tx_size-t->tx_sent){cp_s4_tcp_eof(t);return CP_LK_BAD;}
 t->tx_sent+=n;
 if(t->tx_sent==t->tx_size){
  int rc=cp_lk_consume(&t->session->link,t->tx+8,t->tx_size-8);
  if(rc!=CP_LK_OK){cp_s4_tcp_eof(t);return rc;}
  t->tx_sent=t->tx_size=0;
 }
 return CP_LK_OK;
}
int cp_s4_tcp_tick(struct cp_s4_tcp_bridge *t,uint64_t now){
 if(!authorized(t)){cp_s4_tcp_eof(t);return CP_LK_OWNER;}
 int r=cp_s4_tick(t->session,now);
 if(r!=CP_LK_OK&&r!=CP_LK_BACKPRESSURE){cp_s4_tcp_eof(t);}
 return r;
}
