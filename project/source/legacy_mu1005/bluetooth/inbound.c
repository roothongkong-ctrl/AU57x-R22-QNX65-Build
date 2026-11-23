#include "inbound.h"
_Static_assert(sizeof(SdpAttr)==12,"ARM32 SDP attribute size");
_Static_assert(sizeof(RfObject)==176,"RF object storage");
_Static_assert(sizeof(RfPacket)==100,"TX descriptor storage");
static void zero(void *p,unsigned n){uint8_t *b=p;while(n--) *b++=0;}
static void copy(void *d,const void *s,unsigned n){uint8_t *a=d;const uint8_t*b=s;while(n--) *a++=*b++;}
static void put16(void*p,unsigned off,unsigned v){uint8_t*b=p;b[off]=v;b[off+1]=v>>8;}
static void put32(void*p,unsigned off,uint32_t v){uint8_t*b=p;for(unsigned i=0;i<4;i++)b[off+i]=(uint8_t)(v>>(8*i));}
/* Bluetooth service-class and protocol descriptor data elements. Separate
 * local wireless-iAP service (CAFF) from remote iAP client search (CAFE). */
static const uint8_t iap[19]={0x35,0x11,0x1c,0,0,0,0,0xde,0xca,0xfa,0xde,0xde,0xca,0xde,0xaf,0xde,0xca,0xca,0xff};
static const uint8_t proto[14]={0x35,0x0c,0x35,0x03,0x19,0x01,0,0x35,0x05,0x19,0,0x03,0x08,0};
static const uint8_t cp[19]={0x35,0x11,0x1c,0xec,0x88,0x43,0x48,0xcd,0x41,0x40,0xa2,0x97,0x27,0x57,0x5d,0x50,0xbf,0x1f,0xd3};
static int ok(Inbound*s){return s&&s->ops.certify&&s->ops.certify(s->ops.opaque)==1;}
int inbound_prepare(Inbound*s,const InboundOps*o,uint32_t cb){
 if(!s||!o||s->state!=IN_EMPTY||!cb||!o->reg||!o->unreg||!o->add||!o->remove||!o->accept||!o->limit||!o->send||!o->disconnect||!o->certify||!o->peer_ok||!o->ready||!o->opened||!o->closed||!o->rx||!o->credit||!o->addr)return -1;
 if(o->certify(o->opaque)!=1)return -1;
 s->ops=*o;zero(&s->rf,RF_SIZE);zero(&s->packet,PACKET_SIZE);zero(&s->sdp,SDP_SIZE);zero(&s->carplay,SDP_SIZE);zero(s->attrs,sizeof s->attrs);zero(&s->carattr,sizeof s->carattr);
 s->slot=s->busy=s->pending=s->published_mask=s->uncertain_mask=0;copy(s->svc,iap,19);copy(s->protocol,proto,14);copy(s->cpsvc,cp,19);
 uint32_t ctx=o->addr(s), a=o->addr(s->attrs),p=o->addr(s->protocol),v=o->addr(s->svc),ca=o->addr(&s->carattr),cv=o->addr(s->cpsvc);
 if(!ctx||!a||!p||!v||!ca||!cv)return -1;
 put32(&s->rf,8,cb);put16(&s->rf,12,MAX_DATA);put32(&s->rf,20,ctx);
 s->attrs[0]=(SdpAttr){1,19,v,0,0};s->attrs[1]=(SdpAttr){4,14,p,0,0};s->carattr=(SdpAttr){1,19,cv,0,0};
 ((uint8_t*)&s->sdp)[8]=2;put32(&s->sdp,12,a);((uint8_t*)&s->carplay)[8]=1;put32(&s->carplay,12,ca);
 s->state=IN_PREPARED;return 0;
}
int inbound_publish(Inbound*s){
 if(!ok(s)||s->state!=IN_PREPARED||s->ops.ready(s->ops.opaque)!=1)return -1;
 /* Upstream donor uses zero third argument; not initial RX credits. */
 s->uncertain_mask=IN_RF;
 int r=s->ops.reg(&s->rf,&s->slot,0);
 if(r||s->slot<MU_RFCOMM_SCN_MIN||s->slot>MU_RFCOMM_SCN_MAX){s->state=IN_DIRTY;return -2;}
 s->published_mask=IN_RF;s->uncertain_mask=0;
 /* RFCOMM server channels use the full 1..30 range. A previous donor-only
 * 1..6 cap rejected valid OEM-assigned channels such as 7. The returned
 * channel must agree with SDP ProtocolDescriptorList before publication. */
 s->state=IN_REGISTERED;s->protocol[13]=s->slot;
 /* Do not assume a nonzero vendor return means nothing was mutated. Preserve
  * storage, flag dirty, require owner-supervised rollback on partial failure. */
 s->uncertain_mask=IN_IAP_SDP;
 if(s->ops.add(&s->sdp)){s->state=IN_DIRTY;return -2;}
 s->published_mask|=IN_IAP_SDP;s->uncertain_mask=IN_CP_SDP;
 if(s->ops.add(&s->carplay)){s->state=IN_DIRTY;return -2;}
 s->published_mask|=IN_CP_SDP;s->uncertain_mask=0;
 s->state=IN_PUBLISHED;return 0;
}
int inbound_event(Inbound*s,void*ch,unsigned code,unsigned status,const uint8_t*data,unsigned n,uint32_t packet_addr){
 if(!ok(s)||ch!=&s->rf||s->state<IN_REGISTERED||s->state>=IN_DIRTY)return -1;
 if(code==1){int allow=!status&&s->state==IN_PUBLISHED&&!s->pending&&!s->busy&&s->ops.peer_ok(s->ops.opaque)==1&&s->ops.ready(s->ops.opaque)==1;
  if(allow)s->pending=1;
  if(s->ops.accept(ch,allow)!=2){s->state=IN_DIRTY;return -2;}return allow?1:0;}
 if(code==2){if(status||s->state!=IN_PUBLISHED||!s->pending)return -1;int mtu=s->ops.limit(ch);if(mtu<=0||mtu>MAX_DATA||s->ops.opened(s->ops.opaque,(unsigned)mtu)!=1){s->state=IN_DIRTY;return -2;}s->pending=0;s->state=IN_LINK;/* Bootstrap RX window only after transport opened. */if(s->ops.credit(ch,4)!=0){s->state=IN_DIRTY;return -2;}return 0;}
 if(code==4){s->pending=0;if(s->ops.closed(s->ops.opaque)!=1){s->state=IN_DIRTY;return -2;}/* Do not reuse an in-flight packet on close without OEM completion. */s->state=s->busy?IN_DIRTY:IN_PUBLISHED;return 0;}
 if(code==5){if(s->state!=IN_LINK||status!=0||!data||!n||n>MAX_DATA)return -1;if(s->ops.rx(s->ops.opaque,data,n)!=1){s->state=IN_DIRTY;return -2;}/* R5 defers refill until a whole frame has been written to TCP. */if(!s->ops.defer_rx_credit&&s->ops.credit(ch,1)!=0){s->state=IN_DIRTY;return -2;}return 0;}
 if(code==6){if(!s->busy||packet_addr!=s->ops.addr(&s->packet))return -1;s->busy=0;if(status){s->state=IN_DIRTY;return -2;}return 0;}
 return -1;
}
int inbound_send(Inbound*s,const uint8_t*data,unsigned n){
 if(!ok(s)||s->state!=IN_LINK||s->busy||!data||!n||n>MAX_DATA)return -1;
 int limit=s->ops.limit(&s->rf);if(limit<=0||n>(unsigned)limit)return -1;
 uint32_t pos=s->ops.addr(s->tx);if(!pos)return -1;zero(&s->packet,PACKET_SIZE);copy(s->tx,data,n);put32(&s->packet,8,pos);put16(&s->packet,12,n);s->busy=1;
 if(s->ops.send(&s->rf,&s->packet)!=2){s->state=IN_DIRTY;return -2;}return 0; /* queued, not delivered */
}

int inbound_withdraw(Inbound *s,int (*barrier)(void*,const Inbound*),void *opaque){
 /* A failed add/remove may still change OEM lists. Never guess membership. */
 if(!ok(s)||!barrier||s->state!=IN_PUBLISHED||s->pending||s->busy||s->uncertain_mask||s->published_mask!=(IN_RF|IN_IAP_SDP|IN_CP_SDP)||barrier(opaque,s)!=1)return -1;
 s->state=IN_WITHDRAWING;
 /* The caller holds a proven native serialization barrier for this entire
  * operation. The vendor functions manage their own SDK mutex internally. */
 s->uncertain_mask=IN_CP_SDP;
 if(s->ops.remove(&s->carplay))goto failed;
 s->published_mask&=~IN_CP_SDP;s->uncertain_mask=IN_IAP_SDP;
 if(s->ops.remove(&s->sdp))goto failed;
 s->published_mask&=~IN_IAP_SDP;s->uncertain_mask=IN_RF;
 if(s->ops.unreg(&s->rf,&s->slot))goto failed;
 s->published_mask&=~IN_RF;s->uncertain_mask=0;s->state=IN_RETIRED;
 /* RETIRED does not authorize storage free or republish. */
 return 0;
failed:s->state=IN_DIRTY;return -2;
}
