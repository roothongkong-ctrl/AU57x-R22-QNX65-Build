#include "inbound_r5.h"
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
static int cert(void *p){InboundR5*b=p;return b->vendor.certify(b->vendor.opaque)==1;}
static int ready(void *p){InboundR5*b=p;return b->initialized&&!b->dead&&!b->had_link&&!b->ipc.failed&&!b->rf.failed&&b->vendor.ready(b->vendor.opaque)==1;}
static int peer_ok(void *p){InboundR5*b=p;return b->authorized;}
static int opened(void *p,unsigned mtu){InboundR5*b=p;if(!b->authorized||b->had_link)return 0;
 if(au_rf_connected(&b->rf,b->peer)||au_rf_set_max_payload(&b->rf,mtu))return 0;
 b->had_link=1;return 1;}
static int closed(void *p){InboundR5*b=p;b->authorized=0;return !b->rf.connected||au_rf_disconnected(&b->rf)==0;}
static int rx(void *p,const uint8_t *v,unsigned n){InboundR5*b=p;return !b->dead&&au_rf_received(&b->rf,v,n)==0;}
static int tx(void *p,const uint8_t *v,size_t n){InboundR5*b=p;return inbound_send(&b->service,v,(unsigned)n);}
static int valid_peer(const uint8_t p[6]){unsigned any=0,all=255;for(int i=0;i<6;i++){any|=p[i];all&=p[i];}return any&&all!=255;}
static int mark_dead(InboundR5*b){b->dead=1;au_ipc_socket_lost(&b->ipc);return -1;}
static int abort_stream(InboundR5*b){
 mark_dead(b);(void)shutdown(b->fd,SHUT_RDWR);
 /* Only the certified owner tick calls native teardown. A return value of 2
  * means queued, not closed. Retain packet/service objects until real close. */
 if(b->native_link&&!b->disconnect_attempted){
  b->disconnect_attempted=1;
  b->disconnect_rc=b->vendor.disconnect(&b->service.rf);
 }
 return -1;
}
int ir5_init(InboundR5*b,const InboundOps*o,ResolvePeer res,int fd,uint32_t cb){
 struct sockaddr_in sa; socklen_t n=sizeof sa;
 if(!b||b->initialized||!o||!o->certify||!o->ready||!res||fd<0||o->certify(o->opaque)!=1||getpeername(fd,(struct sockaddr*)&sa,&n)||n!=sizeof sa||sa.sin_family!=AF_INET||sa.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||sa.sin_port!=htons(57342))return -1;
#if !defined(MSG_NOSIGNAL) && !defined(SO_NOSIGPIPE)
 return -1;
#elif !defined(MSG_NOSIGNAL)
 int yes=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof yes))return -1;
#endif
 int fl=fcntl(fd,F_GETFL,0);if(fl<0||fcntl(fd,F_SETFL,fl|O_NONBLOCK)<0)return -1;
 memset(b,0,sizeof *b);b->vendor=*o;b->fd=fd;b->resolve=res;au_rf_init(&b->rf,tx,b);au_ipc_init(&b->ipc,&b->rf);
 InboundOps ops=*o;ops.opaque=b;ops.certify=cert;ops.ready=ready;ops.peer_ok=peer_ok;ops.opened=opened;ops.closed=closed;ops.rx=rx;ops.defer_rx_credit=1;
 if(inbound_prepare(&b->service,&ops,cb))return -1;
 b->initialized=1;return 0;
}
int ir5_publish(InboundR5*b){if(!b||!b->initialized||b->dead)return -1;return inbound_publish(&b->service);}
int ir5_event(InboundR5*b,void*ch,const void*raw,unsigned code,unsigned status,const uint8_t*data,unsigned len,uint32_t packet){
 if(!b||!b->initialized||!cert(b)||ch!=&b->service.rf)return -1;
 /* Even after bridge loss retain pending SDK storage. Never reset the object. */
 /* A genuine close can arrive after the bridge failed. Record it, but do
  * not recycle any descriptor or re-publish service storage here. */
 if(code==4)b->native_link=0;
 if(code==2&&!status&&b->service.pending)b->native_link=1;
 if(b->dead)return -1;
 if(code==1||code==2){uint8_t peer[6]={0};int valid=raw&&b->resolve(b->vendor.opaque,ch,raw,peer)==1&&valid_peer(peer);
  if(code==1){/* An additional incoming event must not replace an accepted peer. */
   if(b->service.pending||b->service.state==IN_LINK)return -1;
   b->authorized=valid;if(valid)memcpy(b->peer,peer,6);
  }else if(!valid||!b->authorized||memcmp(peer,b->peer,6)||status){b->service.state=IN_DIRTY;return mark_dead(b);}
 }
 int r=inbound_event(&b->service,ch,code,status,data,len,packet);
 if(r<0&&b->service.state==IN_DIRTY)return mark_dead(b);
 if(code==6&&r==0)b->completion=1; /* owner tick handles next TX, not callback */
 return r;
}
int ir5_tick(InboundR5*b){
 if(!b||!b->initialized||!cert(b))return -1;
 if(b->dead)return abort_stream(b);
 if(b->completion){b->completion=0;if(au_socket_tx_complete(&b->ipc,1)<0)return abort_stream(b);}
 /* Do not poll/send from OEM callback; binder invokes this only at a safe
  * owner-thread dispatch point. No socket reconnect or automatic teardown. */
 if(au_socket_step(&b->ipc,b->fd,0)<0)return abort_stream(b);
 /* Replenish only after complete DATA frame write, not merely admission to
  * the bounded queue. Owner tick, never a blocking call in RX callback.
  * TCP commit means kernel accepted bytes, not proof of daemon processing. */
 uint32_t count=b->ipc.rx_frames_committed-b->credits_committed;
 if(count){
  b->credits_committed=b->ipc.rx_frames_committed;
  if(b->service.state==IN_LINK&&b->native_link&&b->rf.connected){
   if(count>AU_RF_QSIZE+1u||b->vendor.credit(&b->service.rf,(int)count)!=0){b->service.state=IN_DIRTY;return abort_stream(b);}
  }
 }
 if(b->had_link&&!b->rf.connected&&!b->rf.count&&!b->ipc.out_pending)return abort_stream(b);
 return 0;
}

int ir5_withdraw(InboundR5*b,int (*barrier)(void*,const Inbound*),void*opaque){
 if(!b||!b->initialized||!cert(b)||b->native_link||b->rf.connected||b->service.pending||b->service.busy||b->native_rx_busy||b->completion)return -1;
 int r=inbound_withdraw(&b->service,barrier,opaque);
 if(r==0||r==-2){mark_dead(b);(void)shutdown(b->fd,SHUT_RDWR);}
 return r;
}
