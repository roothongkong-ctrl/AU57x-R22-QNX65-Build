#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "cprx_service.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
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
static void release_unix_path(MuCprxService *s){
 struct stat st;
 /* Never unlink a pathname substituted by another process. */
 if(s->unix_listener&&s->unix_path[0]&&
    lstat(s->unix_path,&st)==0&&
    st.st_dev==s->unix_dev&&st.st_ino==s->unix_ino){
  (void)unlink(s->unix_path);
 }
 s->unix_path[0]=0;
}
/* QNX 6.5 ARM-v7 uses getpeereid() for AF_UNIX SOCK_STREAM peers;
 * Linux host fixtures instead read kernel SO_PEERCRED. Any error denies. */
static int kernel_peer(int fd,uid_t *uid,gid_t *gid){
 struct sockaddr_storage sa; socklen_t len=(socklen_t)sizeof sa;
 if(fd<0||!uid||!gid||getsockname(fd,(struct sockaddr*)&sa,&len)||
    sa.ss_family!=AF_UNIX)return 0;
#ifdef __QNXNTO__
 return getpeereid(fd,uid,gid)==0;
#elif defined(__linux__) && defined(SO_PEERCRED)
 struct ucred creds;socklen_t n=(socklen_t)sizeof creds;
 if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&creds,&n)||n!=sizeof creds)
  return 0;
 *uid=creds.uid;*gid=creds.gid;return 1;
#else
 return 0;
#endif
}
static int private_dir(const char *dir){
 struct stat st;
 size_t n;
 if(!dir||dir[0]!='/'||(n=strlen(dir))<2||n>80||
    dir[n-1]=='/'||strstr(dir,"//")||strstr(dir,"/../")||
    strstr(dir,"/./")||!strcmp(dir,"/..")||!strcmp(dir,"/."))return 0;
 return lstat(dir,&st)==0&&S_ISDIR(st.st_mode)&&
        st.st_uid==geteuid()&&(st.st_mode&0777)==0700;
}
static void drop(MuCprxService *s){if(s->client>=0)close(s->client);s->client=-1;s->used=s->size=s->sent=0;s->required=16;s->mutating=0;s->length_used=s->payload_used=s->payload_need=0;}
/* A client can disappear while OEM START or STOP is returning pending.
 * Dropping it without checking mutating would orphan the original receiver.
 * Shutdown preserves the R77 Unix-path lease until OEM restore succeeds. */
static int abandoned(MuCprxService *s){
 int mutation=s->mutating||s->cleaning;
 drop(s);
 return mutation?mu_cprx_shutdown(s):0;
}
static void reply(MuCprxService *s,unsigned code){memcpy(s->reply,"CPRA\2\0\0\0",8);s->reply[5]=(unsigned char)code;s->size=8;s->sent=0;}
static uint32_t gen(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t epoch64(const unsigned char *p){return (uint64_t)gen(p)|((uint64_t)gen(p+4)<<32);}
static void put32(unsigned char*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void put64(unsigned char*p,uint64_t v){put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32));}
static int peer_verified(const MuCprxService *s,int fd){
 uid_t uid;gid_t gid;
 if(!s||fd<0)return 0;
 if(s->version!=3)return 1;
 return s->unix_listener&&kernel_peer(fd,&uid,&gid)&&
  uid==s->peer_uid&&gid==s->peer_gid&&
  s->evidence.si_client_verified&&
  s->evidence.si_client_verified(s->evidence.opaque,fd,s->generation,
                                 s->epoch,s->session)==1;
}
static int authorized(MuCprxService *s){
 return s->evidence.admitted(s->evidence.opaque,s->generation)==1 &&
  (s->version==2 || (s->evidence.si_admitted && s->evidence.si_admitted(s->evidence.opaque,s->generation,s->epoch,s->session)==1));
}
static int open_service(MuCprxService *s,const MuReceiverBackend *b,
 const MuCprxEvidence *e,unsigned port,unsigned version,uint64_t epoch,
 uint32_t session,const char *dir,uid_t expected_uid,gid_t expected_gid){
 /* A running or not-yet-restored receiver must never be overwritten by a
  * second open in a different private directory. Otherwise the first
  * listener and OEM restoration lease would be lost. Caller zero-initializes
  * MuCprxService; reuse is permitted only after completed release. */
 if(!s||(s->backend.restore&&!s->backend_released))return -1;
 if(!b||!b->opaque||!b->generation||!b->start||!b->poll||!b->restore||!e||!e->admitted||!e->identity||!e->media||!port||port>65535||e->admitted(e->opaque,b->generation)!=1)return -1;
 if(version==3&&(!epoch||!session||!e->si_admitted||
   !e->si_client_verified||
   e->si_admitted(e->opaque,b->generation,epoch,session)!=1))return -1;
 int fd=-1;struct stat inode;char path[MU_CPRX_UNIX_PATH_MAX]={0};
 if(version==3){
  struct sockaddr_un a;size_t n;int flags;
  /* No fallback to AF_INET: a loopback address proves no OS peer. */
  if(!private_dir(dir))return -1;
  n=strlen(dir)+sizeof("/cprx.sock");
  if(n>sizeof a.sun_path||n>sizeof path)return -1;
  (void)snprintf(path,sizeof path,"%s/cprx.sock",dir);
  if(lstat(path,&inode)==0||errno!=ENOENT)return -1; /* never unlink stale */
  fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return -1;
  flags=fcntl(fd,F_GETFD);
  if(flags<0||fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0||nonblock(fd)){
   close(fd);return -1;
  }
  memset(&a,0,sizeof a);a.sun_family=AF_UNIX;
  memcpy(a.sun_path,path,strlen(path)+1);
#ifdef __QNXNTO__
  /* QNX 6.5 UNIX(4): SUN_LEN excludes the terminating NUL. */
  a.sun_len=(unsigned char)SUN_LEN(&a);
  if(bind(fd,(struct sockaddr*)&a,(socklen_t)SUN_LEN(&a))){
#else
  if(bind(fd,(struct sockaddr*)&a,
      (socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1))){
#endif
   close(fd);return -1;
  }
  if(chmod(path,0600)||lstat(path,&inode)||!S_ISSOCK(inode.st_mode)||
     inode.st_uid!=geteuid()||listen(fd,2)){
   /* Path is owned by us in our verified private directory. */
   (void)unlink(path);close(fd);return -1;
  }
 }else{
  struct sockaddr_in a;int yes=1;
  fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
  memset(&a,0,sizeof a);
#ifdef __QNXNTO__
  a.sin_len=sizeof a;
#endif
  a.sin_family=AF_INET;a.sin_port=htons(MU_CPRX_PORT);
  a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)||
     nonblock(fd)||bind(fd,(struct sockaddr*)&a,sizeof a)||listen(fd,2)){
   close(fd);return -1;
  }
 }
 memset(s,0,sizeof *s);s->listener=s->client=-1;
 s->backend=*b;s->evidence=*e;s->generation=b->generation;
 s->port=port;s->version=version;s->epoch=epoch;s->session=session;s->required=16;
 if(version==3){
  s->unix_listener=1;s->peer_uid=expected_uid;s->peer_gid=expected_gid;
  s->unix_dev=inode.st_dev;s->unix_ino=inode.st_ino;
  memcpy(s->unix_path,path,strlen(path)+1);
 }
 s->listener=fd;return 0;
}
static int stock_start(void *p,unsigned port){return mu_dio_start(p,port);}
static int stock_poll(void *p){return mu_dio_poll(p);}
static int stock_restore(void *p){MuDioNative *n=p;return n->initialized?mu_dio_restore(n):0;}
static int open_stock(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,
 unsigned port,unsigned version,uint64_t epoch,uint32_t session,
 const char *dir,uid_t uid,gid_t gid){
 if(!n||!n->initialized)return -1;
 MuReceiverBackend b={n,n->ap.generation,stock_start,stock_poll,stock_restore};
 return open_service(s,&b,e,port,version,epoch,session,dir,uid,gid);
}
int mu_cprx_open(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,
 unsigned port){return open_stock(s,n,e,port,2,0,0,NULL,0,0);}
/* Never leave an insecure SI v3 TCP entry reachable from existing callers. */
int mu_cprx_open_si(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,
 unsigned port,uint64_t epoch,uint32_t session){
 (void)s;(void)n;(void)e;(void)port;(void)epoch;(void)session;return -1;
}
int mu_cprx_open_backend_si(MuCprxService *s,const MuReceiverBackend *b,
 const MuCprxEvidence *e,unsigned port,uint64_t epoch,uint32_t session){
 (void)s;(void)b;(void)e;(void)port;(void)epoch;(void)session;return -1;
}
int mu_cprx_open_si_unix(MuCprxService *s,MuDioNative *n,const MuCprxEvidence *e,
 unsigned port,uint64_t epoch,uint32_t session,const char *dir,uid_t uid,gid_t gid){
 return open_stock(s,n,e,port,3,epoch,session,dir,uid,gid);
}
int mu_cprx_open_backend_si_unix(MuCprxService *s,const MuReceiverBackend *b,
 const MuCprxEvidence *e,unsigned port,uint64_t epoch,uint32_t session,
 const char *dir,uid_t uid,gid_t gid){
 return open_service(s,b,e,port,3,epoch,session,dir,uid,gid);
}
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
 /* Keep the original socket pathname reserved while OEM stop/restore is
  * pending, including after closing the listening FD. Otherwise another SI
  * controller could bind the same name and start the singleton prematurely.
  * Retry through this existing service until the native owner confirms its
  * teardown barrier; only THEN remove the socket inode. */
 if(restore_backend(s)!=0)return -1;
 release_unix_path(s);
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
 if(op==MU_CPRX_OP_WIRED_RESTORE){
  /* NEVER use a wire assertion as a wired event. The genuine DIO owner
   * must independently verify the physical wired transition of this exact
   * session. This check repeats during a pending OEM restore. */
  if(!s->evidence.dio_wired_takeover_verified||
     s->evidence.dio_wired_takeover_verified(s->evidence.opaque,
            s->generation,s->epoch,s->session)!=1){
   if(s->mutating)s->cleaning=1; /* finish rollback if already started */
   reply(s,2);return 0;
  }
  s->mutating=1;
  int restored=restore_backend(s);
  if(restored==1)return 0; /* original stop/interface restore pending */
  if(restored<0){s->cleaning=1;reply(s,2);return 0;}
  s->started=s->start_confirmed=0;s->terminal=1;
  reply(s,0);return 0; /* only after OEM backend released */
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
 /* R83: same-session DIO-side proof snapshot; callbacks MUST originate
  * from verified original owner/session events, not current media alone.
  * Report absent/unknown evidence as RETRY, never as streaming ready. */
 if(op==MU_CPRX_OP_SESSION_STATUS&&s->version==3){
  int m,auth;unsigned mask=0;
  if(!s->evidence.si_inner_iap2_mfi||!s->evidence.si_media_channel){
   reply(s,2);return 0;
  }
  m=s->evidence.media(s->evidence.opaque);
  if(m!=0&&m!=4&&m!=5&&m!=7){reply(s,1);return 0;}
  auth=s->evidence.si_inner_iap2_mfi(s->evidence.opaque,
           s->generation,s->epoch,s->session);
  if(auth!=0&&auth!=1){reply(s,1);return 0;}
  if(auth==1&&m==7){
   for(unsigned channel=1;channel<=4;channel++){
    int live=s->evidence.si_media_channel(s->evidence.opaque,
        s->generation,s->epoch,s->session,channel);
    /* Optional media owners may not yet be implemented. Unknown audio,
     * MMI or steering is absence of evidence, never a ready bit. Keep
     * genuine video evidence independent for the first-connect client.
     * Unknown video is also an unset bit, never a ready bit. Full/video
     * clients still require the corresponding bits; session-only clients do not. */
    if(live==-1)continue;
    if(live!=0&&live!=1){reply(s,1);return 0;}
    if(live==1)mask|=1u<<(channel-1);
   }
  }
  reply(s,0);
  s->reply[8]=(unsigned char)m;
  s->reply[9]=(unsigned char)auth;
  s->reply[10]=(unsigned char)mask;
  s->reply[11]=0;
  put32(s->reply+12,s->generation);
  put64(s->reply+16,s->epoch);
  put32(s->reply+24,s->session);
  s->size=28;
  return 0;
 }
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
  /* Admission happens BEFORE accepting request bytes or mutating DIO.
   * Even a correctly formatted CPRX session token is not an OS peer proof. */
  if(!peer_verified(s,fd)||nonblock(fd)||safe_send(fd)){close(fd);return 0;}
  s->client=fd;s->deadline=now+6500;}
#if defined(__QNXNTO__) && !defined(MU_CPRX_QNX_IMMEDIATE_CLOSE)
 /* QNX may invalidate getpeername when the sender closes before the
  * receiver reads its queued reply. Keep the authenticated stream open
  * until the receiver closes, bounded by the existing transaction deadline.
  * Never accept another request on this completed stream. */
 if(s->version==3&&s->size&&s->sent==s->size){
  unsigned char extra;
  ssize_t n=recv(s->client,&extra,1,MSG_PEEK);
  if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)&&
     now<s->deadline&&peer_verified(s,s->client))return 0;
  drop(s);
  return s->terminal||s->cleaning?mu_cprx_shutdown(s):0;
 }
#endif
 if(!peer_verified(s,s->client)){
  /* The local SI process may have exited/relinquished this lease.
   * If it was in a native mutation, preserve rollback semantics. */
  return abandoned(s);
 }
 if(now>=s->deadline)return abandoned(s);
 if(s->used<s->required){ssize_t r=recv(s->client,s->req+s->used,s->required-s->used,0);if(r<=0){if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;return abandoned(s);}s->used+=(unsigned)r;if(s->used<s->required)return 0;
  if(s->required==16 && s->req[4]==3 && s->version==3){s->required=32;return 0;}}
 /* v3 semantic messages carry a 2-byte length or a fixed 4-byte ACK. */
 unsigned op=s->req[5];
 if(!s->size&&(!header_valid(s) || op>MU_CPRX_OP_WIRED_RESTORE || (op>=8&&s->version!=3) || (op==8&&!s->evidence.metadata) || (op==9&&!s->evidence.semantic_in) || (op==10&&!s->evidence.semantic_next)))reply(s,2);
 if(!s->size&&op>=8&&op<=10){
  if(op==10)s->payload_need=4;
  else if(s->length_used<2){ssize_t r=recv(s->client,s->length_bytes+s->length_used,2-s->length_used,0);if(r<=0){if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;return abandoned(s);}s->length_used+=(unsigned)r;if(s->length_used<2)return 0;s->payload_need=(unsigned)s->length_bytes[0]|(unsigned)s->length_bytes[1]<<8;if(s->payload_need<6||s->payload_need>(op==8?4096u:8192u))reply(s,2);}
  if(!s->size&&s->payload_used<s->payload_need){ssize_t r=recv(s->client,s->payload+s->payload_used,s->payload_need-s->payload_used,0);if(r<=0){if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;return abandoned(s);}s->payload_used+=(unsigned)r;if(s->payload_used<s->payload_need)return 0;}
 }
 if(!s->size){/* Detect abandoned pending request before touching OEM again. */
  unsigned char extra;ssize_t r=recv(s->client,&extra,1,MSG_PEEK);if(r>=0)return abandoned(s);
  if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)return abandoned(s);
  handle(s);if(!s->size)return 0;}
 ssize_t w=send(s->client,s->reply+s->sent,s->size-s->sent,MSG_NOSIGNAL);if(w>0){s->sent+=(unsigned)w;if(s->sent==s->size){if(op==1&&s->reply[5]==0&&s->started)s->start_confirmed=1;
#if defined(__QNXNTO__) && !defined(MU_CPRX_QNX_IMMEDIATE_CLOSE)
  if(s->version==3&&!s->cleaning){s->mutating=0;return 0;}
#endif
  drop(s);if(s->terminal||s->cleaning)return mu_cprx_shutdown(s);}return 0;}
 if(w<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;
 return s->terminal?mu_cprx_shutdown(s):abandoned(s);
}

/* R78: the production SI control sender (previously absent). Same original
 * owner thread calls client_begin/tick and services mu_cprx_tick; the AF_UNIX
 * server still independently checks the kernel credentials and SI owner.
 * No additional source module, QNX thread, fake iPhone or trust in wire IDs. */
void mu_cprx_client_init(MuCprxClient *c){
 if(!c)return;
 memset(c,0,sizeof *c);c->fd=-1;c->observed_media=-1;c->observed_inner=-1;
}
void mu_cprx_client_close(MuCprxClient *c){
 if(!c)return;
 if(c->phase&&c->fd>=0)close(c->fd);
 mu_cprx_client_init(c);
}
static int client_fail(MuCprxClient *c){
 if(!c)return MU_CPRX_CLIENT_ERROR;
 if(c->fd>=0)close(c->fd);
 c->fd=-1;c->phase=5;
 c->observed_media=-1;c->observed_inner=-1;c->observed_channels=0;
 return MU_CPRX_CLIENT_ERROR;
}
static int client_begin_op(MuCprxClient *c,MuCprxService *s,uint64_t now,
                           unsigned operation){
 struct sockaddr_un addr;struct stat inode;
 int fd,flags,rc;
 uid_t uid;gid_t gid;
 size_t len;
 if(!c||c->phase||!s||s->version!=3||!s->unix_listener||
    (operation!=1&&operation!=2)||
    s->listener<0||s->backend_released||s->terminal||s->cleaning||
    (operation==1&&(s->started||s->start_confirmed))||
    (operation==2&&(!s->started||!s->start_confirmed))||
    !s->generation||!s->epoch||!s->session||!s->port||s->port>65535||
    !s->unix_path[0]||!s->evidence.si_admitted||
    s->evidence.si_admitted(s->evidence.opaque,s->generation,
                            s->epoch,s->session)!=1||
    !s->evidence.admitted||
    s->evidence.admitted(s->evidence.opaque,s->generation)!=1||
    lstat(s->unix_path,&inode)!=0||!S_ISSOCK(inode.st_mode)||
    inode.st_dev!=s->unix_dev||inode.st_ino!=s->unix_ino)return -1;
 len=strlen(s->unix_path);
 if(len>=sizeof addr.sun_path)return -1;
 fd=socket(AF_UNIX,SOCK_STREAM,0);
 if(fd<0)return -1;
 flags=fcntl(fd,F_GETFD);
 if(flags<0||fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0||
    nonblock(fd)||safe_send(fd)){
  /* QNX needs SO_NOSIGPIPE when MSG_NOSIGNAL is unavailable; do not risk
   * a disconnected SI peer delivering SIGPIPE to the original process. */
  close(fd);return -1;
 }
 memset(&addr,0,sizeof addr);addr.sun_family=AF_UNIX;
 memcpy(addr.sun_path,s->unix_path,len+1);
#ifdef __QNXNTO__
 addr.sun_len=(unsigned char)SUN_LEN(&addr);
 rc=connect(fd,(struct sockaddr*)&addr,(socklen_t)SUN_LEN(&addr));
#else
 rc=connect(fd,(struct sockaddr*)&addr,
   (socklen_t)(offsetof(struct sockaddr_un,sun_path)+len+1));
#endif
 if(rc<0&&errno!=EINPROGRESS&&errno!=EAGAIN&&errno!=EWOULDBLOCK){
  close(fd);return -1;
 }
 /* Validate the server's kernel identity as well; the owner callback is
  * separately checked on the receiving side. Same UID is NOT a PID proof. */
 if(rc==0&&(!kernel_peer(fd,&uid,&gid)||
    uid!=s->peer_uid||gid!=s->peer_gid)){
  close(fd);return -1;
 }
 mu_cprx_client_init(c);
 c->service=s;c->fd=fd;c->phase=rc<0?1:2;c->operation=operation;
 c->reply_need=8;
 c->generation=s->generation;c->epoch=s->epoch;c->session=s->session;
 c->deadline=now+6500;
 memcpy(c->request,"CPRX",4);c->request[4]=3;
 c->request[5]=(unsigned char)operation;
 put32(c->request+8,s->generation);
 c->request[12]=(unsigned char)s->port;
 c->request[13]=(unsigned char)(s->port>>8);
 put32(c->request+16,(uint32_t)s->epoch);
 put32(c->request+20,(uint32_t)(s->epoch>>32));
 put32(c->request+24,s->session);
 return 0;
}
int mu_cprx_client_begin(MuCprxClient *c,MuCprxService *s,uint64_t now){
 return client_begin_op(c,s,now,1);
}
int mu_cprx_client_begin_stop(MuCprxClient *c,MuCprxService *s,uint64_t now){
 return client_begin_op(c,s,now,2);
}
/* R81: caller runs in original SI process; the DIO socket lives in an
 * independent address space. Never copy or dereference a DIO service pointer.
 * The endpoint MUST come from an independently authenticated OEM SI/DIO
 * association, never from a message received on this socket. */
int mu_cprx_client_begin_remote(MuCprxClient *c,
 const MuCprxRemoteEndpoint *endpoint,unsigned op,uint64_t now){
 struct sockaddr_un addr;
 struct stat st,dir_st;
 char dir[MU_CPRX_UNIX_PATH_MAX];
 const char *slash;
 size_t n,dir_len;
 int fd,flags,rc;
 uid_t uid;gid_t gid;
 if(!c||c->phase||!endpoint||(op!=1&&op!=2&&op!=MU_CPRX_OP_STATUS&&op!=MU_CPRX_OP_SESSION_STATUS&&op!=MU_CPRX_OP_WIRED_RESTORE)||
    !endpoint->generation||!endpoint->epoch||!endpoint->session||
    !endpoint->airplay_port||endpoint->airplay_port>65535||
    !endpoint->si_owner_verified||!endpoint->dio_server_verified||
    endpoint->si_owner_verified(endpoint->owner_ctx,endpoint->generation,
          endpoint->epoch,endpoint->session)!=1||
    (op==MU_CPRX_OP_WIRED_RESTORE&&
      (!endpoint->si_wired_takeover_verified||
       endpoint->si_wired_takeover_verified(endpoint->owner_ctx,
       endpoint->generation,endpoint->epoch,endpoint->session)!=1)))return -1;
 for(n=0;n<sizeof endpoint->socket_path&&endpoint->socket_path[n];n++){}
 /* Some QNX 6.5 libc builds lack strnlen(); this bound is exact. */
 if(n<12||n>=sizeof addr.sun_path||endpoint->socket_path[0]!='/'||
    strcmp(endpoint->socket_path+n-10,"/cprx.sock"))return -1;
 slash=strrchr(endpoint->socket_path,'/');
 if(!slash||slash==endpoint->socket_path)return -1;
 dir_len=(size_t)(slash-endpoint->socket_path);
 if(dir_len>=sizeof dir)return -1;
 memcpy(dir,endpoint->socket_path,dir_len);dir[dir_len]=0;
 /* Private path + pinned server effective owner. SI and DIO might use
  * different OS accounts on the actual car: that remains unverified. */
 if(lstat(dir,&dir_st)!=0||!S_ISDIR(dir_st.st_mode)||
    (dir_st.st_mode&0777)!=0700||dir_st.st_uid!=endpoint->server_uid||
    lstat(endpoint->socket_path,&st)!=0||!S_ISSOCK(st.st_mode)||
    (st.st_mode&0777)!=0600||st.st_uid!=endpoint->server_uid)return -1;
 fd=socket(AF_UNIX,SOCK_STREAM,0);
 if(fd<0)return -1;
 flags=fcntl(fd,F_GETFD);
 if(flags<0||fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0||
    nonblock(fd)||safe_send(fd)){close(fd);return -1;}
 memset(&addr,0,sizeof addr);addr.sun_family=AF_UNIX;
 memcpy(addr.sun_path,endpoint->socket_path,n+1);
#ifdef __QNXNTO__
 addr.sun_len=(unsigned char)SUN_LEN(&addr);
 rc=connect(fd,(struct sockaddr*)&addr,(socklen_t)SUN_LEN(&addr));
#else
 rc=connect(fd,(struct sockaddr*)&addr,
     (socklen_t)(offsetof(struct sockaddr_un,sun_path)+n+1));
#endif
 if(rc<0&&errno!=EINPROGRESS&&errno!=EAGAIN&&errno!=EWOULDBLOCK){
  close(fd);return -1;
 }
 if(rc==0&&(!kernel_peer(fd,&uid,&gid)||
     uid!=endpoint->server_uid||gid!=endpoint->server_gid||
     endpoint->dio_server_verified(endpoint->owner_ctx,fd,
       endpoint->generation,endpoint->epoch,endpoint->session)!=1)){
  close(fd);return -1;
 }
 mu_cprx_client_init(c);
 c->remote_mode=1;c->remote=*endpoint;
 c->fd=fd;c->phase=rc<0?1:2;c->operation=op;
 c->reply_need=8u; /* status may return an 8-byte BUSY response */
 c->generation=endpoint->generation;
 c->epoch=endpoint->epoch;c->session=endpoint->session;
 c->deadline=now+6500;
 memcpy(c->request,"CPRX",4);c->request[4]=3;c->request[5]=(unsigned char)op;
 put32(c->request+8,endpoint->generation);
 c->request[12]=(unsigned char)endpoint->airplay_port;
 c->request[13]=(unsigned char)(endpoint->airplay_port>>8);
 put32(c->request+16,(uint32_t)endpoint->epoch);
 put32(c->request+20,(uint32_t)(endpoint->epoch>>32));
 put32(c->request+24,endpoint->session);
 return 0;
}
int mu_cprx_client_tick(MuCprxClient *c,uint64_t now){
 MuCprxService *s;
 uid_t uid;gid_t gid;
 if(!c||!c->phase)return MU_CPRX_CLIENT_ERROR;
 if(c->phase==5)return MU_CPRX_CLIENT_ERROR;
 if(c->phase==6)return MU_CPRX_CLIENT_BUSY;
 s=c->service;
 if(c->remote_mode){
  const MuCprxRemoteEndpoint *e=&c->remote;
  if(s||!e->si_owner_verified||!e->dio_server_verified||
     e->generation!=c->generation||e->epoch!=c->epoch||
     e->session!=c->session||
     e->si_owner_verified(e->owner_ctx,e->generation,e->epoch,e->session)!=1||
     (c->operation==MU_CPRX_OP_WIRED_RESTORE&&
       (!e->si_wired_takeover_verified||
        e->si_wired_takeover_verified(e->owner_ctx,
              e->generation,e->epoch,e->session)!=1)))
   return client_fail(c);
 }else{
  if(!s||s->generation!=c->generation||s->epoch!=c->epoch||
     s->session!=c->session||!s->evidence.si_admitted||
     s->evidence.si_admitted(s->evidence.opaque,s->generation,
                            s->epoch,s->session)!=1)
   return client_fail(c);
 }
 /* Remote DIO process ownership must remain independently verified; a
  * successful connect and kernel UID alone are insufficient for a car. */
 if(c->remote_mode&&c->phase>=2&&c->phase<=3&&
    c->remote.dio_server_verified(c->remote.owner_ctx,c->fd,
        c->generation,c->epoch,c->session)!=1)return client_fail(c);
 /* An old START ACK isn't permanent proof of a live SI session. A STOP
  * ACK must be backed by a completed native restoration, never only bytes. */
 if(c->phase==4){
  if(!c->remote_mode){
   if(c->operation==1&&(!s->started||!s->start_confirmed||
                         s->terminal||s->backend_released))return client_fail(c);
   if(c->operation==2&&!s->backend_released)return client_fail(c);
  }
  /* For remote clients, an ACK reports what the authenticated DIO service
   * confirmed at that instant, not continued streaming or a permanent lease. */
  return MU_CPRX_CLIENT_CONFIRMED;
 }
 if(now>=c->deadline||
    (!c->remote_mode&&
      ((c->operation==1&&(s->terminal||s->backend_released))||
       (c->operation==2&&s->backend_released&&c->phase!=3))))
  return client_fail(c);
 if(c->phase==1){
  int error=0;socklen_t n=(socklen_t)sizeof error;
  if(getsockopt(c->fd,SOL_SOCKET,SO_ERROR,&error,&n)<0)
   return client_fail(c);
  if(error==EINPROGRESS||error==EALREADY)return MU_CPRX_CLIENT_PENDING;
  if(error)return client_fail(c);
  if(!kernel_peer(c->fd,&uid,&gid)||
     uid!=(c->remote_mode?c->remote.server_uid:s->peer_uid)||
     gid!=(c->remote_mode?c->remote.server_gid:s->peer_gid)||
     (c->remote_mode&&c->remote.dio_server_verified(
         c->remote.owner_ctx,c->fd,c->generation,c->epoch,c->session)!=1))
   return client_fail(c);
  c->phase=2;
 }
 if(c->phase==2){
  ssize_t n=send(c->fd,c->request+c->sent,32-c->sent,MSG_NOSIGNAL);
  if(n<0){
   if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)
    return MU_CPRX_CLIENT_PENDING;
   return client_fail(c);
  }
  if(n==0)return client_fail(c);
  c->sent+=(unsigned)n;
  if(c->sent!=32)return MU_CPRX_CLIENT_PENDING;
  c->phase=3;
 }
 if(c->phase==3){
  unsigned need=c->reply_need;
  /* Avoid parsing an ACK while its media-state payload is still in flight. */
  if(need!=8&&need!=12&&need!=28)return client_fail(c);
  ssize_t n=recv(c->fd,c->reply+c->received,need-c->received,0);
  if(n<0){
   if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)
    return MU_CPRX_CLIENT_PENDING;
   return client_fail(c);
  }
  if(n==0)return client_fail(c);
  c->received+=(unsigned)n;
  if(c->received!=need)return MU_CPRX_CLIENT_PENDING;
  if(memcmp(c->reply,"CPRA",4)||c->reply[4]!=2||
     c->reply[6]||c->reply[7])return client_fail(c);
  /* DIO status has a VARIABLE response size: 8-byte RETRY/error, but
   * exactly 12 bytes only for a success that carries stock media state.
   * Never wait for four bytes the server will not send on RETRY. */
  if(need==8&&c->reply[5]==0){
   if(c->operation==MU_CPRX_OP_STATUS){
    c->reply_need=12;
    return MU_CPRX_CLIENT_PENDING;
   }
   if(c->operation==MU_CPRX_OP_SESSION_STATUS){
    c->reply_need=28;
    return MU_CPRX_CLIENT_PENDING;
   }
  }
  /* A DIO receiver can be booting: an op 6 request returns RETRY/1,
   * never invent media=7 or report a completed status query. */
  if(c->operation==MU_CPRX_OP_STATUS||c->operation==MU_CPRX_OP_SESSION_STATUS){
   if(c->reply[5]==1){
    close(c->fd);c->fd=-1;c->phase=6;
    return MU_CPRX_CLIENT_BUSY;
   }
   if(!c->remote_mode||c->reply[5]!=0||
      (c->reply[8]!=0&&c->reply[8]!=4&&
       c->reply[8]!=5&&c->reply[8]!=7))
    return client_fail(c);
   if(c->operation==MU_CPRX_OP_STATUS){
    if(need!=12||c->reply[9]||c->reply[10]||c->reply[11])
     return client_fail(c);
   }else{
    if(need!=28||c->reply[9]>1||(c->reply[10]&0xf0)||
       c->reply[11]||gen(c->reply+12)!=c->generation||
       epoch64(c->reply+16)!=c->epoch||gen(c->reply+24)!=c->session||
       (c->reply[8]!=7&&c->reply[10])||
       (!c->reply[9]&&c->reply[10]))return client_fail(c);
    c->observed_inner=(int)c->reply[9];
    c->observed_channels=(unsigned)c->reply[10];
   }
   c->observed_media=(int)c->reply[8];
  }else{
   if(c->operation==2&&c->reply[5]==3){
    close(c->fd);c->fd=-1;c->phase=6;
    return MU_CPRX_CLIENT_BUSY;
   }
   if(c->reply[5]!=0||
      (!c->remote_mode&&c->operation==1&&(!s->start_confirmed||!s->started))||
      (!c->remote_mode&&c->operation==2&&!s->backend_released))
    return client_fail(c);
  }
  close(c->fd);c->fd=-1;c->phase=4;
  return MU_CPRX_CLIENT_CONFIRMED;
 }
 return MU_CPRX_CLIENT_PENDING;
}
