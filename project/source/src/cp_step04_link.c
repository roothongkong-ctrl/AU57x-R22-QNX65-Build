/* SPDX-License-Identifier: GPL-3.0-only
 * C99 translation of the narrow link-control behaviour in:
 * luka-dev/mib2q-carplay-rgi/wireless/daemon/vendor/iap2-link/src/lib.rs
 * Adapted for AU57x exact-epoch RFCOMM queues; intentionally no firmware I/O.
 * Out-of-order frames are ACKed only at the last in-order PSN; no media/EA.
 */
#include "cp_step04_link.h"
#include <string.h>
#define LK_SYN 0x80u
#define LK_ACK 0x40u
#define LK_RST 0x10u
#define LK_TX_RETRY 4000u
#define LK_SYN_RETRY 500u
#define LK_DETECT_RETRY 1000u
static const uint8_t marker[6]={0xff,0x55,0x02,0x00,0xee,0x10};
static uint16_t be16(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
static void put16(uint8_t *p,size_t n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static uint8_t checksum(const uint8_t *p,size_t n){
 uint8_t x=0;for(size_t i=0;i<n;i++)x=(uint8_t)(x+p[i]);return (uint8_t)(0u-x);
}
static int valid_sum(const uint8_t*p,size_t n){return checksum(p,n)==0;}
void cp_lk_init(struct cp_step04_link *l){if(l)memset(l,0,sizeof *l);}
void cp_lk_revoke(struct cp_step04_link *l){
 if(!l)return;
 l->phase=CP_LK_DEAD;l->outbound_pending=0;l->count=l->head=l->tail=0;
 l->rx_used=0;l->outbound_len=0;l->outbound_token=0;
 memset(l->outbound_data,0,sizeof l->outbound_data);
}
static int live(struct cp_step04_link *l){
 if(!l||l->phase==CP_LK_DEAD||l->phase==CP_LK_IDLE)return 0;
 if(!l->owner.live || l->owner.live(l->owner.opaque,l->epoch,l->generation)!=1){
  cp_lk_revoke(l);return 0;
 }
 return 1;
}
static int enqueue(struct cp_step04_link *l,const uint8_t*p,size_t n){
 if(!p||!n||n>CP_LK_FRAME_MAX||l->count>=CP_LK_OUTPUT_SLOTS)return CP_LK_BACKPRESSURE;
 struct cp_lk_wire *w=&l->output[l->tail];w->size=n;
 memcpy(w->data,p,n);l->tail=(l->tail+1u)%CP_LK_OUTPUT_SLOTS;++l->count;
 return CP_LK_OK;
}
static int emit(struct cp_step04_link *l,uint8_t control,uint8_t seq,
                uint8_t sid,const uint8_t *data,size_t len){
 uint8_t out[CP_LK_FRAME_MAX];size_t total=len?len+10u:9u;
 if(total>sizeof out)return CP_LK_BAD;
 out[0]=0xff;out[1]=0x5a;put16(out+2,total);
 out[4]=control;out[5]=seq;out[6]=l->rx_psn;out[7]=sid;
 out[8]=checksum(out,8);
 if(len){memcpy(out+9,data,len);out[9+len]=checksum(data,len);}
 return enqueue(l,out,total);
}
static int syn(struct cp_step04_link *l){
 /* Upstream LSP layout: version, max_out, max_len, RTO, ack delay,
  * max_retransmit, ack threshold, session ID/kind/version. */
 const uint8_t lsp[]={1,4,0x05,0x80,0x0f,0xa0,0x01,0xf4,4,3,10,0,1};
 return emit(l,LK_SYN,l->tx_psn,0,lsp,sizeof lsp);
}
static int begin(struct cp_step04_link *l,const uint8_t peer[6],uint32_t epoch,
                 uint32_t generation,const struct cp_lk_owner *o,uint64_t now){
 if(!l||!peer||!o||!o->live||!o->control||!o->acked||!epoch||!generation||
    l->phase!=CP_LK_IDLE)return CP_LK_BAD;
 cp_lk_init(l);l->owner=*o;l->epoch=epoch;l->generation=generation;
 l->phase=CP_LK_DETECT;l->tx_psn=99;
 memcpy(l->peer_mac,peer,6);
 if(!live(l))return CP_LK_OWNER;
 if(enqueue(l,marker,sizeof marker)!=CP_LK_OK){cp_lk_revoke(l);return CP_LK_BACKPRESSURE;}
 l->deadline=now+LK_DETECT_RETRY;return CP_LK_OK;
}
static int peer_lsp(const uint8_t *data,size_t n){
 /* Reject truncated LSPs, unknown version and peers lacking control session. */
 if(n<13||((n-10u)%3u)!=0||data[0]!=1||!data[1]||
    be16(data+2)<10||!be16(data+4)||!data[8])return 0;
 for(size_t i=10;i+2<n;i+=3)if(data[i]==CP_LK_CONTROL_ID && data[i+1]==0)return 1;
 return 0;
}
static int process(struct cp_step04_link *l,const uint8_t*h,const uint8_t*pl,
                   size_t n,uint64_t now){
 uint8_t flags=h[4],seq=h[5],ack=h[6],sid=h[7];
 if(flags&LK_RST){cp_lk_revoke(l);return CP_LK_DEAD_LINK;}
 if(flags&LK_SYN){
  if(l->phase!=CP_LK_NEGOTIATE||sid!=0||!peer_lsp(pl,n)){
   cp_lk_revoke(l);return CP_LK_BAD;
  }
  l->have_peer_syn=1;l->rx_psn=seq;
  if(emit(l,LK_ACK,l->tx_psn,0,NULL,0)!=CP_LK_OK){cp_lk_revoke(l);return CP_LK_BACKPRESSURE;}
 }
 if(flags&LK_ACK){
  if(l->phase==CP_LK_NEGOTIATE&&l->have_peer_syn&&ack==l->tx_psn){
   l->phase=CP_LK_NORMAL;l->deadline=0;
  }
  if(l->phase==CP_LK_NORMAL&&l->outbound_pending&&ack==l->outbound_seq){
   uint32_t token=l->outbound_token;
   l->outbound_pending=0;l->outbound_token=0;l->outbound_len=0;
   if(l->owner.acked(l->owner.opaque,token)!=0){cp_lk_revoke(l);return CP_LK_OWNER;}
  }
 }
 if(flags==LK_ACK&&n){
  if(l->phase!=CP_LK_NORMAL||sid!=CP_LK_CONTROL_ID||!l->have_peer_syn||
     n<6||pl[0]!=0x40||pl[1]!=0x40||be16(pl+2)!=n){
   cp_lk_revoke(l);return CP_LK_BAD;
  }
  if(seq==l->rx_psn){
   return emit(l,LK_ACK,l->tx_psn,0,NULL,0);
  }
  if((uint8_t)(l->rx_psn+1u)!=seq){
   /* Future/out-of-order packets are not delivered to MFi; ACK prior PSN. */
   return emit(l,LK_ACK,l->tx_psn,0,NULL,0);
  }
  if(l->owner.control(l->owner.opaque,pl,n)!=0){cp_lk_revoke(l);return CP_LK_OWNER;}
  l->rx_psn=seq;
  if(emit(l,LK_ACK,l->tx_psn,0,NULL,0)!=CP_LK_OK){cp_lk_revoke(l);return CP_LK_BACKPRESSURE;}
 }
 (void)now;
 return CP_LK_OK;
}
int cp_lk_feed_bytes(struct cp_step04_link *l,const uint8_t *data,size_t n,uint64_t now){
 if(!l||!data||!n||n>CP_LK_RX_MAX)return CP_LK_BAD;
 if(!live(l))return CP_LK_OWNER;
 if(l->rx_used+n>sizeof l->rx){cp_lk_revoke(l);return CP_LK_BAD;}
 memcpy(l->rx+l->rx_used,data,n);l->rx_used+=n;
 while(1){
  if(l->phase==CP_LK_DETECT){
   if(l->rx_used<sizeof marker)return CP_LK_OK;
   if(memcmp(l->rx,marker,sizeof marker)!=0){cp_lk_revoke(l);return CP_LK_BAD;}
   memmove(l->rx,l->rx+sizeof marker,l->rx_used-sizeof marker);
   l->rx_used-=sizeof marker;l->phase=CP_LK_NEGOTIATE;
   if(syn(l)!=CP_LK_OK){cp_lk_revoke(l);return CP_LK_BACKPRESSURE;}
   l->deadline=now+LK_SYN_RETRY;
  }
  if(l->rx_used<9)return CP_LK_OK;
  if(l->rx[0]!=0xff||l->rx[1]!=0x5a||!valid_sum(l->rx,9)){
   /* Only discard one byte; a later valid header may recover sync. */
   memmove(l->rx,l->rx+1,--l->rx_used);continue;
  }
  size_t total=be16(l->rx+2);
  if(total!=9&&(total<10||total>CP_LK_FRAME_MAX)){
   memmove(l->rx,l->rx+1,--l->rx_used);continue;
  }
  if(l->rx_used<total)return CP_LK_OK;
  if(total>9&&!valid_sum(l->rx+9,total-9)){
   memmove(l->rx,l->rx+total,l->rx_used-total);
   l->rx_used-=total;continue;
  }
  uint8_t hdr[9];memcpy(hdr,l->rx,9);
  uint8_t payload[CP_LK_FRAME_MAX];size_t plen=total>9?total-10:0;
  if(plen)memcpy(payload,l->rx+9,plen);
  memmove(l->rx,l->rx+total,l->rx_used-total);l->rx_used-=total;
  int r=process(l,hdr,plen?payload:NULL,plen,now);
  if(r!=CP_LK_OK)return r;
 }
}
int cp_lk_accept_rf(struct cp_step04_link *l,const AuRfWire *w,uint32_t epoch,
                    const struct cp_lk_owner *o,uint64_t now){
 if(!l||!w||w->len<8||w->len>AU_RF_WIRE_MAX||w->bytes[1]!=0)return CP_LK_BAD;
 size_t n=(size_t)w->bytes[2]|((size_t)w->bytes[3]<<8);
 uint32_t gen=(uint32_t)w->bytes[4]|((uint32_t)w->bytes[5]<<8)|
   ((uint32_t)w->bytes[6]<<16)|((uint32_t)w->bytes[7]<<24);
 if(n+8!=w->len||!gen)return CP_LK_BAD;
 if(w->bytes[0]==1){
  if(n!=6)return CP_LK_BAD;
  return begin(l,w->bytes+8,epoch,gen,o,now);
 }
 if(gen!=l->generation||epoch!=l->epoch)return CP_LK_STALE;
 if(w->bytes[0]==3&&n==0){cp_lk_revoke(l);return CP_LK_OK;}
 if(w->bytes[0]==2&&n)return cp_lk_feed_bytes(l,w->bytes+8,n,now);
 return CP_LK_BAD;
}
int cp_lk_queue_control(struct cp_step04_link *l,const uint8_t *msg,size_t n,
                        uint32_t token,uint64_t now){
 if(!l||!msg||!token||n<6||n>CP_LK_FRAME_MAX-10||
    msg[0]!=0x40||msg[1]!=0x40||be16(msg+2)!=n)return CP_LK_BAD;
 if(!live(l))return CP_LK_OWNER;
 if(l->phase!=CP_LK_NORMAL||l->outbound_pending)return CP_LK_BACKPRESSURE;
 uint8_t next=(uint8_t)(l->tx_psn+1u);
 int r=emit(l,LK_ACK,next,CP_LK_CONTROL_ID,msg,n);
 if(r!=CP_LK_OK)return r;
 l->tx_psn=next;l->outbound_pending=1;l->outbound_seq=next;
 l->outbound_token=token;l->outbound_len=n;
 memcpy(l->outbound_data,msg,n);l->deadline=now+LK_TX_RETRY;l->retries=0;
 return CP_LK_OK;
}
int cp_lk_tick(struct cp_step04_link *l,uint64_t now){
 if(!live(l))return CP_LK_OWNER;
 if(!l->deadline||now<l->deadline)return CP_LK_OK;
 int r=CP_LK_OK;
 if(l->phase==CP_LK_DETECT){
  if(l->negotiate_tries>=3){cp_lk_revoke(l);return CP_LK_DEAD_LINK;}
  r=enqueue(l,marker,sizeof marker);if(r==CP_LK_OK){l->negotiate_tries++;l->deadline=now+LK_DETECT_RETRY;}
 } else if(l->phase==CP_LK_NEGOTIATE){
  if(l->negotiate_tries>=4){cp_lk_revoke(l);return CP_LK_DEAD_LINK;}
  r=syn(l);if(r==CP_LK_OK){l->negotiate_tries++;l->deadline=now+LK_SYN_RETRY;}
 } else if(l->phase==CP_LK_NORMAL&&l->outbound_pending){
  if(l->retries>=4){cp_lk_revoke(l);return CP_LK_DEAD_LINK;}
  r=emit(l,LK_ACK,l->outbound_seq,CP_LK_CONTROL_ID,
         l->outbound_data,l->outbound_len);
  if(r==CP_LK_OK){l->retries++;l->deadline=now+LK_TX_RETRY;}
 } else l->deadline=0;
 return r;
}
int cp_lk_peek(const struct cp_step04_link *l,const uint8_t **p,size_t *n){
 if(!l||!p||!n||!l->count)return 0;
 *p=l->output[l->head].data;*n=l->output[l->head].size;return 1;
}
int cp_lk_consume(struct cp_step04_link *l,const uint8_t *data,size_t n){
 if(!l||!data||!l->count)return CP_LK_BAD;
 if(!live(l))return CP_LK_OWNER;
 const struct cp_lk_wire *w=&l->output[l->head];
 if(!n||w->size!=n||memcmp(w->data,data,n)!=0)return CP_LK_BAD;
 l->head=(l->head+1u)%CP_LK_OUTPUT_SLOTS;--l->count;
 return CP_LK_OK;
}
int cp_lk_drain_rf(struct cp_step04_link *l,AuRfIngress *rf){
 if(!l||!rf)return CP_LK_BAD;
 if(!live(l))return CP_LK_OWNER;
 if(!rf->connected||rf->failed||rf->generation!=l->generation)return CP_LK_STALE;
 if(!l->count)return CP_LK_OK;
 if(rf->tx_busy||!rf->max_payload)return CP_LK_BACKPRESSURE;
 const struct cp_lk_wire *w=&l->output[l->head];
 uint8_t normalized[8+CP_LK_FRAME_MAX];size_t n=w->size;
 if(!n||n>CP_LK_FRAME_MAX)return CP_LK_BAD;
 normalized[0]=2;normalized[1]=0;normalized[2]=(uint8_t)n;
 normalized[3]=(uint8_t)(n>>8);
 for(int i=0;i<4;i++)normalized[4+i]=(uint8_t)(rf->generation>>(8*i));
 memcpy(normalized+8,w->data,n);
 if(au_rf_daemon_data(rf,normalized,n+8)!=0){cp_lk_revoke(l);return CP_LK_DEAD_LINK;}
 return cp_lk_consume(l,w->data,n);
}
