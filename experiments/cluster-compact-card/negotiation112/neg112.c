/* Testable 112 display announcement and independent SETUP TCP lifecycle.
 * SOURCE-ONLY / NO INSTALL. This is NOT wired into stock AirPlayCopyServerInfo,
 * SessionSetup, or SessionTearDown on the target QNX head unit.
 * Host proof flags are synthetic, not evidence of an iPhone capability.
 */
#include "neg112.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define N112_URL "maps:/car/instrumentcluster/instructioncard"
static int valid_uuid(const char *p){
 int j; if(!p||strlen(p)!=36)return 0;
 for(j=0;j<36;++j){
  char c=p[j];
  if(j==8||j==13||j==18||j==23){if(c!='-')return 0;}
  else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||
            (c>='A'&&c<='F')))return 0;
 }
 return 1;
}
void n112_init(struct n112_session *s){
 if(!s)return;
 memset(s,0,sizeof(*s));s->listener=-1;s->client=-1;
 c112_init(&s->receiver);
}
void n112_destroy(struct n112_session *s){
 if(!s)return;
 if(s->client>=0){shutdown(s->client,SHUT_RDWR);close(s->client);}
 if(s->listener>=0){shutdown(s->listener,SHUT_RDWR);close(s->listener);}
 c112_end(&s->receiver);n112_init(s);
}
int n112_prepare(struct n112_session *s,const struct n112_proof *p,
    uint64_t sid,const char *uuid,const char *map_uuid,
    unsigned w,unsigned h,c112_decrypt_t decrypt,void *crypto){
 struct sockaddr_in sa;socklen_t n=sizeof(sa);
 int fd;
 if(!s||!p)return N112_INVALID;
 if(s->offered||s->listener>=0||s->started)return N112_DUPLICATE;
 if(!p->explicit_opt_in||!p->ios_112_observed||
    !p->distinct_aes_context||!p->codec_and_decoder_ready||
    !p->overlay_alpha_ready||!p->full_and_partial_teardown_verified)
    return N112_NO_PROOF;
 if(!sid||!w||!h||w>2048||h>1024||!decrypt||
    !valid_uuid(uuid)||!valid_uuid(map_uuid)||!strcmp(uuid,map_uuid))
    return N112_INVALID;
 fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 if(fd<0)return N112_SOCKET;
 memset(&sa,0,sizeof(sa));sa.sin_family=AF_INET;
 /* LAB ONLY. A real CarPlay listener must bind the verified CarPlay interface,
  * and must not be enabled until QNX security/session tests pass. */
 sa.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 sa.sin_port=0;
 if(bind(fd,(struct sockaddr*)&sa,sizeof(sa))<0||
    listen(fd,1)<0||getsockname(fd,(struct sockaddr*)&sa,&n)<0){
  close(fd);return N112_SOCKET;
 }
 s->listener=fd;s->session_id=sid;s->port=ntohs(sa.sin_port);
 s->display.type=112;s->display.width=w;s->display.height=h;
 s->display.width_mm=200;s->display.height_mm=200*h/w;
 s->display.max_fps=30;
 memcpy(s->display.uuid,uuid,37);
 strcpy(s->display.initial_url,N112_URL);
 s->receiver.decrypt=decrypt;s->receiver.crypto=crypto;
 s->offered=1;return N112_OK;
}
int n112_offer(const struct n112_session *s,struct n112_descriptor *out){
 if(!s||!out||!s->offered||s->listener<0)return N112_NOT_READY;
 *out=s->display;return N112_OK;
}
int n112_setup(struct n112_session *s,unsigned type,uint64_t sid,
    uint64_t cid,struct n112_setup *response){
 c112_decrypt_t cb;void *ctx;int rc;
 if(type==110||type==111)return N112_NOT_READY; /* original owner handles both */
 if(!s||!response||type!=112||!cid||!sid)return N112_INVALID;
 if(!s->offered||sid!=s->session_id||s->listener<0)return N112_STALE;
 if(s->started)return N112_DUPLICATE;
 cb=s->receiver.decrypt;ctx=s->receiver.crypto;
 if(!cb)return N112_NO_PROOF;
 rc=c112_begin(&s->receiver,sid,cid,cb,ctx);
 if(rc!=0)return N112_NO_PROOF;
 s->connection_id=cid;s->started=1;
 response->type=112;response->stream_connection_id=cid;
 response->data_port=s->port;
 return N112_OK;
}
int n112_accept(struct n112_session *s){
 int c;
 if(!s||!s->started||s->listener<0||s->client>=0)return N112_NOT_READY;
 do{c=accept(s->listener,NULL,NULL);}while(c<0&&errno==EINTR);
 if(c<0)return N112_SOCKET;
 s->client=c;return N112_OK;
}
int n112_receive_one(struct n112_session *s){
 int rc;
 if(!s||!s->started||s->client<0)return N112_NOT_READY;
 rc=c112_receive_one(&s->receiver,s->client);
 if(rc<0){ /* close private 112 transport, not 111/110 */
  close(s->client);s->client=-1;s->started=0;s->connection_id=0;
 }
 return rc;
}
int n112_teardown(struct n112_session *s,unsigned type,uint64_t sid,
    uint64_t cid){
 c112_decrypt_t cb;void *ctx;
 if(!s||!sid)return N112_INVALID;
 if(type==110||type==111)return N112_NOT_READY;
 if(type!=112||sid!=s->session_id||!cid||cid!=s->connection_id)
   return N112_STALE;
 cb=s->receiver.decrypt;ctx=s->receiver.crypto;
 if(s->client>=0){shutdown(s->client,SHUT_RDWR);close(s->client);s->client=-1;}
 c112_end(&s->receiver);
 s->receiver.decrypt=cb;s->receiver.crypto=ctx;
 s->started=0;s->connection_id=0;
 /* Keep /info offer socket ready to service another 112 SETUP in this session. */
 return N112_OK;
}
