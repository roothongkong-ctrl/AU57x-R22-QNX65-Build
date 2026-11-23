#include "mu1005_iap2_stream.h"
#include <string.h>
static int authorized(Mu1005Iap2Stream *s){
 return s && s->active && !s->stopped && s->rf && s->authorization &&
  s->generation && s->authorization->generation==s->generation &&
  s->rf->state==IN_LINK && au108_live(s->authorization)==1;
}
static int enqueue(void *ctx,const uint8_t *bytes,size_t count){
 Mu1005Iap2Stream *s=(Mu1005Iap2Stream *)ctx;
 if(!s||!s->active||s->stopped||(!bytes&&count)||!count||
    count>MU1005_IAP2_TX_CAP-s->queued)return 0;
 size_t tail=(s->head+s->queued)%MU1005_IAP2_TX_CAP;
 size_t first=MU1005_IAP2_TX_CAP-tail;
 if(first>count)first=count;
 memcpy(s->fifo+tail,bytes,first);
 if(count>first)memcpy(s->fifo,bytes+first,count-first);
 s->queued+=count;return 1; /* buffered, NEVER SDK ACK */
}
static void discard_rx(void *ctx,uint8_t session,const uint8_t *p,size_t n){
 (void)ctx;(void)session;(void)p;(void)n;
 /* Control-session/MFi callbacks are NOT attached to the OEM until verified.
  * Do not automatically feed any untrusted payload to DIO/CarPlay. */
}
static void discard_state(void *ctx,int state){(void)ctx;(void)state;}
static void fail(Mu1005Iap2Stream *s){
 if(!s)return;
 s->stopped=1;s->queued=0;s->head=0;
 if(s->core)au57x_iap2_reset(s->core); /* no RST through OEM after fault */
}
void mu1005_iap2_stream_init(Mu1005Iap2Stream *s,Inbound *rf,Au108Port *auth){
 if(!s)return;memset(s,0,sizeof(*s));s->rf=rf;s->authorization=auth;
}
int mu1005_iap2_stream_pump(Mu1005Iap2Stream *s){
 if(!authorized(s)){fail(s);return -1;}
 if(!s->queued||s->rf->busy)return 0;
 int limit=s->rf->ops.limit(&s->rf->rf);
 if(limit<=0){fail(s);return -2;}
 size_t n=s->queued;
 if(n>(size_t)limit)n=(size_t)limit;
 if(n>MAX_DATA)n=MAX_DATA;
 if(!n){fail(s);return -2;}
 size_t first=MU1005_IAP2_TX_CAP-s->head;
 if(first>n)first=n;
 memcpy(s->chunk,s->fifo+s->head,first);
 if(n>first)memcpy(s->chunk+first,s->fifo,n-first);
 if(inbound_send(s->rf,s->chunk,(unsigned)n)!=0){fail(s);return -2;}
 s->head=(s->head+n)%MU1005_IAP2_TX_CAP;
 s->queued-=n;return (int)n;
}
int mu1005_iap2_stream_on_open(Mu1005Iap2Stream *s,uint32_t generation){
 if(!s||s->active||s->stopped||!generation||!s->rf||
    s->rf->state!=IN_LINK||!s->authorization||
    s->authorization->generation!=generation||
    au108_live(s->authorization)!=1)return -1;
 s->generation=generation;s->active=1;
 au57x_iap2_callbacks cb={0};
 cb.ctx=s;cb.tx=enqueue;cb.rx=discard_rx;cb.state=discard_state;
 s->core=au57x_iap2_new(&cb);
 if(!s->core||au57x_iap2_start(s->core)!=1){fail(s);return -2;}
 return mu1005_iap2_stream_pump(s)<0?-2:0;
}
int mu1005_iap2_stream_on_bytes(Mu1005Iap2Stream *s,const uint8_t *p,size_t n){
 if(!authorized(s)||!s->core||(!p&&n)||!n||n>MAX_DATA){fail(s);return -1;}
 if(au57x_iap2_feed(s->core,p,n)!=1){fail(s);return -2;}
 return 0; /* owner MUST pump after vendor callback unwinds */
}
int mu1005_iap2_stream_tick(Mu1005Iap2Stream *s,int64_t ms){
 if(!authorized(s)||!s->core||ms<0){fail(s);return -1;}
 if(au57x_iap2_tick(s->core,ms)!=1){fail(s);return -2;}
 return mu1005_iap2_stream_pump(s)<0?-2:0;
}
void mu1005_iap2_stream_stop(Mu1005Iap2Stream *s){
 if(!s)return;
 fail(s);
 if(s->core){au57x_iap2_free(s->core);s->core=0;}
 s->active=0;
 /* Does not unload btstack or remove RF/SDP. OEM owner is responsible. */
}
