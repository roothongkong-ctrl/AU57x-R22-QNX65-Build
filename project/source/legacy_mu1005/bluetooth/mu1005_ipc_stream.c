#include "mu1005_ipc_stream.h"
#include <string.h>
void au_ipc_init(AuIpcStream*s,AuRfIngress*rf){if(!s)return;memset(s,0,sizeof(*s));s->rf=rf;}
static int fail(AuIpcStream*s){s->failed=1;if(s->rf)au_rf_abort(s->rf);return -1;}
int au_ipc_pump(AuIpcStream*s){
 if(!s||!s->rf||s->failed)return -1;
 /* The RF ingress generation advances on disconnect and never wraps to
  * zero: wrap aborts the owner. Fully decoded older-generation records
  * must not kill a fresh connection or reach the new handset. Discard
  * only such records; zero/future/unassigned generations remain fatal. */
 while(s->pending_count){
  AuRfWire*w=&s->pending[s->pending_head];
  uint32_t g=(uint32_t)w->bytes[4]|((uint32_t)w->bytes[5]<<8)|
             ((uint32_t)w->bytes[6]<<16)|((uint32_t)w->bytes[7]<<24);
  if(!g||g>s->rf->generation)return fail(s);
  if(g==s->rf->generation){
   if(!s->rf->connected)return fail(s);
   /* Peer MTU might arrive after CONNECTED. Keep this generation intact. */
   if(s->rf->tx_busy||!s->rf->max_payload)return 0;
   if(au_rf_daemon_data(s->rf,w->bytes,w->len))return fail(s);
   memset(w,0,sizeof(*w));
   s->pending_head=(s->pending_head+1)%AU_RF_QSIZE;--s->pending_count;
   return 0;
  }
  memset(w,0,sizeof(*w));
  s->pending_head=(s->pending_head+1)%AU_RF_QSIZE;--s->pending_count;
 }
 return 0;
}
int au_ipc_feed(AuIpcStream*s,const uint8_t*p,size_t n){
 if(!s||!s->rf||s->failed||(!p&&n))return -1;
 while(n){
  size_t need=(s->expected?s->expected:8)-s->used;
  size_t k=n<need?n:need;memcpy(s->incoming+s->used,p,k);
  s->used+=k;p+=k;n-=k;
  if(!s->expected&&s->used==8){
   size_t len=(size_t)s->incoming[2]|((size_t)s->incoming[3]<<8);
   if(s->incoming[0]!=2||s->incoming[1]!=0||!len||len>AU_RF_MAX_DATA)return fail(s);
   s->expected=8+len;
  }
  if(s->expected&&s->used==s->expected){
   if(s->pending_count==AU_RF_QSIZE)return fail(s);
   AuRfWire*w=&s->pending[s->pending_tail];
   memcpy(w->bytes,s->incoming,s->used);w->len=s->used;
   s->pending_tail=(s->pending_tail+1)%AU_RF_QSIZE;++s->pending_count;
   s->used=s->expected=0;if(au_ipc_pump(s))return -1;
  }
 }
 return 0;
}
int au_ipc_peek(AuIpcStream*s,const uint8_t**data,size_t*len){
 if(!s||!data||!len||s->failed||!s->rf)return -1;
 if(!s->out_pending){
  int rc=au_rf_take(s->rf,&s->outgoing);
  if(rc<=0)return rc;
  s->offset=0;s->out_pending=1;
 }
 *data=s->outgoing.bytes+s->offset;*len=s->outgoing.len-s->offset;return 1;
}
int au_ipc_commit(AuIpcStream*s,size_t written){
 if(!s||s->failed||!s->out_pending||written>s->outgoing.len-s->offset)return -1;
 s->offset+=written;
 if(s->offset==s->outgoing.len){
  if(s->outgoing.bytes[0]==2)++s->rx_frames_committed;
  s->out_pending=0;s->offset=0;memset(&s->outgoing,0,sizeof(s->outgoing));
 }return 0;
}
void au_ipc_socket_lost(AuIpcStream*s){
 if(!s)return;
 s->failed=1;if(s->rf)au_rf_abort(s->rf);
 s->used=s->expected=0;s->out_pending=0;s->offset=0;
 s->pending_head=s->pending_tail=s->pending_count=0;
 memset(s->incoming,0,sizeof(s->incoming));memset(&s->outgoing,0,sizeof(s->outgoing));
}
