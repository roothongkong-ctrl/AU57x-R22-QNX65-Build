#include "mu1005_rf_ingress.h"
#include <string.h>
/* Type, zero reserved, little-endian length, little-endian generation, payload. */
static int queued(AuRfIngress*s,uint8_t kind,const uint8_t*data,size_t n){
 if(s->failed||s->count==AU_RF_QSIZE||n>AU_RF_MAX_DATA){au_rf_abort(s);return -1;}
 AuRfWire*w=&s->queue[s->tail];
 w->bytes[0]=kind;w->bytes[1]=0;w->bytes[2]=(uint8_t)n;w->bytes[3]=(uint8_t)(n>>8);
 for(unsigned i=0;i<4;i++)w->bytes[4+i]=(uint8_t)(s->generation>>(i*8));
 if(n)memcpy(w->bytes+8,data,n);
 w->len=n+8;s->tail=(s->tail+1)%AU_RF_QSIZE;++s->count;return 0;
}
void au_rf_init(AuRfIngress*s,AuRfSdkSend f,void*ctx){if(!s)return;memset(s,0,sizeof(*s));s->sdk_send=f;s->sdk_ctx=ctx;}
void au_rf_abort(AuRfIngress*s){if(!s)return;s->failed=1;s->connected=0;s->tx_cancelled=1;s->head=s->tail=s->count=0;/* Do NOT free busy SDK-owned storage before async completion. */}
int au_rf_connected(AuRfIngress*s,const uint8_t peer[6]){
 if(!s||!peer||s->failed||s->connected||s->count||s->tx_busy)return -1;
 if(++s->generation==0){au_rf_abort(s);return -1;}
 memcpy(s->peer,peer,6);s->max_payload=0;s->connected=1;s->tx_cancelled=0;
 return queued(s,1,peer,6);
}
int au_rf_set_max_payload(AuRfIngress*s,size_t cap){
 if(!s||s->failed||!s->connected||s->tx_busy||!cap||cap>AU_RF_MAX_DATA)return -1;
 s->max_payload=cap;return 0;
}
int au_rf_received(AuRfIngress*s,const void*bytes,size_t n){
 if(!s||!s->connected||!bytes||!n||n>AU_RF_MAX_DATA){au_rf_abort(s);return -1;}
 return queued(s,2,bytes,n);
}
int au_rf_disconnected(AuRfIngress*s){
 if(!s||!s->connected||s->failed)return -1;
 s->connected=0;s->max_payload=0;
 if(s->tx_busy)s->tx_cancelled=1;
 else s->tx_len=s->tx_offset=s->tx_inflight=0;
 if(queued(s,3,0,0))return -1;
 if(++s->generation==0){au_rf_abort(s);return -1;}return 0;
}
int au_rf_take(AuRfIngress*s,AuRfWire*wire){
 if(!s||s->failed||!wire||!s->count)return 0;
 *wire=s->queue[s->head];s->head=(s->head+1)%AU_RF_QSIZE;--s->count;return 1;
}
static int send_next(AuRfIngress*s){
 size_t n=s->tx_len-s->tx_offset;
 if(!s->connected||s->tx_cancelled||!s->max_payload||!n)return -1;
 if(n>s->max_payload)n=s->max_payload;
 s->tx_inflight=n;
 /* SDK callback is asynchronous; it MUST NOT reenter this object. */
 if(s->sdk_send(s->sdk_ctx,s->tx+s->tx_offset,n)!=0){au_rf_abort(s);return -1;}
 return 0;
}
int au_rf_daemon_data(AuRfIngress*s,const uint8_t*wire,size_t len){
 if(!s||s->failed||!s->connected||!s->sdk_send||!s->max_payload||s->tx_busy||
   !wire||len<9||len>AU_RF_WIRE_MAX||wire[0]!=2||wire[1]!=0)return -1;
 size_t n=(size_t)wire[2]|((size_t)wire[3]<<8);
 uint32_t g=(uint32_t)wire[4]|((uint32_t)wire[5]<<8)|((uint32_t)wire[6]<<16)|((uint32_t)wire[7]<<24);
 if(!n||n>AU_RF_MAX_DATA||len!=n+8||g!=s->generation)return -1;
 memcpy(s->tx,wire+8,n);s->tx_busy=1;s->tx_cancelled=0;s->tx_len=n;s->tx_offset=0;return send_next(s);
}
int au_rf_send_complete(AuRfIngress*s,int ok){
 if(!s||!s->tx_busy)return -1;
 if(!ok){au_rf_abort(s);return -1;}
 if(s->failed||s->tx_cancelled||!s->connected){
  s->tx_busy=0;s->tx_len=s->tx_offset=s->tx_inflight=0;return s->failed?-1:0;
 }
 s->tx_offset+=s->tx_inflight;
 if(s->tx_offset<s->tx_len)return send_next(s);
 /* Full normalized frame only: no receipt for partial/cancelled sends. */
 if(s->tx_receipt.sequence==UINT64_MAX){
  s->tx_busy=0;s->tx_len=s->tx_offset=s->tx_inflight=0;
  au_rf_abort(s);return -1;
 }
 ++s->tx_receipt.sequence;
 s->tx_receipt.generation=s->generation;
 s->tx_receipt.bytes=s->tx_len;
 s->tx_busy=0;s->tx_len=s->tx_offset=s->tx_inflight=0;return 0;
}
int au_rf_tx_receipt(const AuRfIngress*s,uint32_t g,uint64_t after,AuRfTxReceipt*out){
 if(!out)return 0;
 memset(out,0,sizeof(*out));
 if(!s||!g||s->failed||!s->connected||s->tx_cancelled||
    s->generation!=g||s->tx_receipt.generation!=g||
    after==UINT64_MAX||s->tx_receipt.sequence!=after+1u||
    !s->tx_receipt.bytes)return 0;
 *out=s->tx_receipt;return 1;
}
