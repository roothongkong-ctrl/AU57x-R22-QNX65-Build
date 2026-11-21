#define _POSIX_C_SOURCE 200809L
#include "cprx_service.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#ifdef MSG_NOSIGNAL
#define MU_HAVE_MSG_NOSIGNAL 1
#else
#define MSG_NOSIGNAL 0
#endif
static int nonblock(int fd){int f=fcntl(fd,F_GETFL);return f<0?-1:fcntl(fd,F_SETFL,f|O_NONBLOCK);}
static int safe_send(int fd){
#ifdef SO_NOSIGPIPE
 int yes=1;return setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof yes);
#elif defined(MU_HAVE_MSG_NOSIGNAL)
 (void)fd;return 0;
#else
 (void)fd;return -1; /* never risk killing DIO with SIGPIPE */
#endif
}
static void drop(MuCprxService *s){if(s->client>=0)close(s->client);s->client=-1;s->used=s->size=s->sent=0;s->required=16;s->mutating=0;s->length_used=s->payload_used=s->payload_need=0;}
static void reply(MuCprxService *s,unsigned code){memcpy(s->reply,"CPRA\2\0\0\0",8);s->reply[5]=(unsigned char)code;s->size=8;s->sent=0;}
static uint32_t gen(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t epoch64(const unsigned char *p){return (uint64_t)gen(p)|((uint64_t)gen(p+4)<<32);}
static void put32(unsigned char*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static int authorized(MuCprxService *s){
 return s->evidence.admitted(s->evidence.opaque,s->generation)==1 &&
  (s->version==2 || (s->evidence.si_admitted && s->evidence.si_admitted(s->evidence.opaque,s->generation,s->epoch,s->session)==1));
}
static int open_service(MuCprxService *s,const MuReceiverBackend *b,const MuCprxEvidence *e,unsigned port,unsigned version,uint64_t epoch,uint32_t session){
 if(!s||!b||!b->opaque||!b->generation||!b->start||!b->poll||!b->restore||!e||!e->admitted||!e->identity||!e->media||!port||port>65535||e->admitted(e->opaque,b->generation)!=1)return -1;
 if(version==3&&(!epoch||!session||!e->si_admitted||e->si_admitted(e->opaque,b->generation,epoch,session)!=1))return -1;
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
 struct sockaddr_in a;memset(&a,0,sizeof a);
#ifdef __QNXNTO__
 a.sin_len=sizeof a;
#endif
 a.sin_family=AF_INET;a.sin_port=htons(MU_CPRX_PORT);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 int yes=1;if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)||nonblock(fd)||bind(fd,(struct sockaddr*)&a,sizeof a)||listen(fd,2)){close(fd);return -1;}
 memset(s,0,sizeof *s);s->listener=s->client=-1;s->backend=*b;s->evidence=*e;s->generation=b->generation;s->port=port;s->version=version;s->epoch=epoch;s->session=session;s->required=16;
 s->listener=fd;return 0;
}
static int stock_start(void *p,unsigned port){return mu_dio_start(p,port);}
static int stock_poll(void *p){return mu_dio_poll(p);}
static int stock_restore(void *p){MuDioNative *n=p;return n->initialized?mu_dio_restore(n):0;}
static int open_stock(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,unsigned port,unsigned version,uint64_t epoch,uint32_t session){
 if(!n||!n->initialized)return -1;
 MuReceiverBackend b={n,n->ap.generation,stock_start,stock_poll,stock_restore};
 return open_service(s,&b,e,port,version,epoch,session);
}
int mu_cprx_open(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,unsigned port){return open_stock(s,n,e,port,2,0,0);}
int mu_cprx_open_si(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,unsigned port,uint64_t epoch,uint32_t session){return open_stock(s,n,e,port,3,epoch,session);}
int mu_cprx_open_backend_si(MuCprxService *s,const MuReceiverBackend *b,const MuCprxEvidence *e,unsigned port,uint64_t epoch,uint32_t session){return open_service(s,b,e,port,3,epoch,session);}
static int restore_backend(MuCprxService *s){
 if(s->backend_released)return 0;
 int r=s->backend.restore(s->backend.opaque);
 if(r==0)s->backend_released=1;
 return r==0||r==1?r:-1;
}
int mu_cprx_shutdown(MuCprxService *s){
 if(!s||!s->backend.restore)return -1;
 drop(s);if(s->listener>=0){close(s->listener);s->listener=-1;}
 s->terminal=1;
 if(restore_backend(s)!=0)return -1;
 s->started=s->start_confirmed=s->cleaning=0;return 1;
}
static int header_valid(const MuCprxService *s){
 const unsigned char *p=s->req;
 if(memcmp(p,"CPRX",4)||p[4]!=s->version||p[6]||p[7]||p[14]||p[15]||gen(p+8)!=s->generation||((unsigned)p[12]|(unsigned)p[13]<<8)!=s->port)return 0;
 if(s->version==3&&(epoch64(p+16)!=s->epoch||gen(p+24)!=s->session||gen(p+28)))return 0;
 return 1;
}
/* Cross-check live advertised identity instead of trusting listener status alone.
 * Keep the same identity through this admitted generation; no synthetic IDs. */
static int verify_identity(MuCprxService *s){
 char id[253];int n=s->evidence.identity(s->evidence.opaque,id,sizeof id);
 if(n==0)return 0;
 if(n<0||n>252)return -1;
 for(int i=0;i<n;i++)if((unsigned char)id[i]<=32||(unsigned char)id[i]>=127)return -1;
 if(s->identity_len)return s->identity_len==(unsigned)n&&!memcmp(id,s->identity_value,(size_t)n)?1:-1;
 memcpy(s->identity_value,id,(size_t)n);s->identity_len=(unsigned)n;return 1;
}
static int handle(MuCprxService *s){
 unsigned op=s->req[5];
 if(!header_valid(s)){reply(s,2);return 0;}
 if(op==1){
  /* A previously acknowledged START is an observation, not another native
   * transition. Losing a retry's socket must not stop the existing session. */
  s->mutating=!s->start_confirmed;
  int rc=s->started?s->backend.poll(s->backend.opaque):s->backend.start(s->backend.opaque,s->port);s->started=1;
  if(rc==2){int id=verify_identity(s);if(id==1){reply(s,0);return 0;}if(id==0)return 0;rc=-1;}
  if(rc==1)return 0;
  if(!s->start_confirmed)s->cleaning=1;
  reply(s,2);return 0;
 }
 if(op==2){
  /* A lost bootstrap link must not tear down already streaming IP media.
   * The native SI owner retains the lease on BUSY/unknown. Only begin STOP
   * with a known idle/ended observation. Once stop is submitted, finish its
   * barrier without restarting the decision on subsequent owner ticks. */
  if(s->started&&!s->mutating){
   int m=s->evidence.media(s->evidence.opaque);
   if(m==7){reply(s,3);return 0;}
   if(m!=0&&m!=4&&m!=5){reply(s,1);return 0;}
  }
  s->mutating=1;int r=restore_backend(s);if(r==1)return 0;if(r<0){s->cleaning=1;reply(s,2);return 0;}s->started=s->start_confirmed=0;s->terminal=1;reply(s,0);return 0;}
 if(!s->started||s->backend.poll(s->backend.opaque)!=2){reply(s,1);return 0;}
 int id=verify_identity(s);if(id!=1){reply(s,id==0?1:2);return 0;}
 if(op==5){
  reply(s,0);s->reply[8]=(unsigned char)s->identity_len;s->reply[9]=0;memcpy(s->reply+10,s->identity_value,s->identity_len);s->size=s->identity_len+10;return 0;}
 if(op==6||op==7){int m=s->evidence.media(s->evidence.opaque);if(m!=0&&m!=4&&m!=5&&m!=7){reply(s,1);return 0;}reply(s,0);s->reply[8]=(unsigned char)m;memset(s->reply+9,0,3);s->size=12;return 0;}
 if(op>=8&&op<=10&&s->version==3){int r=-1;
  if(op==8&&s->evidence.metadata&&mu_sem_csm(s->payload,s->payload_need))r=s->evidence.metadata(s->evidence.opaque,s->payload,s->payload_need);
  if(op==9&&s->evidence.semantic_in)r=mu_sem_input(&s->semantic,s->payload,s->payload_need,s->evidence.opaque,s->evidence.semantic_in);
  if(op==10&&s->evidence.semantic_next){r=mu_sem_output(&s->semantic,gen(s->payload),s->evidence.opaque,s->evidence.semantic_next);
   if(r>=0){reply(s,0);put32(s->reply+8,s->semantic.output_seq);s->reply[12]=(unsigned char)r;s->reply[13]=(unsigned char)((unsigned)r>>8);memcpy(s->reply+14,s->semantic.output,(size_t)r);s->size=14+(unsigned)r;return 0;}}
  reply(s,r==1?0:r==0?1:2);return 0;}
 reply(s,2);return 0;
}
int mu_cprx_tick(MuCprxService *s,uint64_t now){
 if(!s||!s->backend.restore)return -1;
 if(s->cleaning||(s->terminal&&s->client<0)||!authorized(s))return mu_cprx_shutdown(s);
 if(s->listener<0)return -1;
 if(s->client<0){int fd=accept(s->listener,NULL,NULL);if(fd<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?0:-1;
  if(nonblock(fd)||safe_send(fd)){close(fd);return 0;}s->client=fd;s->deadline=now+6500;}
 if(now>=s->deadline){int mutate=s->mutating;drop(s);return mutate?mu_cprx_shutdown(s):0;}
 if(s->used<s->required){ssize_t r=recv(s->client,s->req+s->used,s->required-s->used,0);if(r<=0){if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;drop(s);return 0;}s->used+=(unsigned)r;if(s->used<s->required)return 0;
  if(s->required==16 && s->req[4]==3 && s->version==3){s->required=32;return 0;}}
 /* v3 semantic messages carry a 2-byte length or a fixed 4-byte ACK. */
 unsigned op=s->req[5];
 if(!s->size&&(!header_valid(s) || op>10 || (op>=8&&s->version!=3) || (op==8&&!s->evidence.metadata) || (op==9&&!s->evidence.semantic_in) || (op==10&&!s->evidence.semantic_next)))reply(s,2);
 if(!s->size&&op>=8&&op<=10){
  if(op==10)s->payload_need=4;
  else if(s->length_used<2){ssize_t r=recv(s->client,s->length_bytes+s->length_used,2-s->length_used,0);if(r<=0){if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;drop(s);return 0;}s->length_used+=(unsigned)r;if(s->length_used<2)return 0;s->payload_need=(unsigned)s->length_bytes[0]|(unsigned)s->length_bytes[1]<<8;if(s->payload_need<6||s->payload_need>(op==8?4096u:8192u))reply(s,2);}
  if(!s->size&&s->payload_used<s->payload_need){ssize_t r=recv(s->client,s->payload+s->payload_used,s->payload_need-s->payload_used,0);if(r<=0){if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;drop(s);return 0;}s->payload_used+=(unsigned)r;if(s->payload_used<s->payload_need)return 0;}
 }
 if(!s->size){/* Detect abandoned pending request before touching OEM again. */
  unsigned char extra;ssize_t r=recv(s->client,&extra,1,MSG_PEEK);if(r>=0){int mutate=s->mutating;drop(s);return mutate?mu_cprx_shutdown(s):0;}
  if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){int mutate=s->mutating;drop(s);return mutate?mu_cprx_shutdown(s):0;}
  handle(s);if(!s->size)return 0;}
 ssize_t w=send(s->client,s->reply+s->sent,s->size-s->sent,MSG_NOSIGNAL);if(w>0){s->sent+=(unsigned)w;if(s->sent==s->size){if(op==1&&s->reply[5]==0&&s->started)s->start_confirmed=1;drop(s);if(s->terminal||s->cleaning)return mu_cprx_shutdown(s);}return 0;}
 if(w<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;
 int mutate=s->mutating;drop(s);return (mutate||s->cleaning||s->terminal)?mu_cprx_shutdown(s):0;
}
