/* SOURCE-ONLY experimental Type-112 transport for v1.2.14.
 * Uses stock 128-byte screen headers and upstream alt111_video parser.
 * Caller must supply independently derived and live AES-CTR decrypt callback.
 * The test XOR callback is not for the vehicle.
 */
#include "c112_ingest.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
static uint32_t le32(const uint8_t *p){
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int read_full(int fd,void *buf,size_t n){
 uint8_t *p=(uint8_t*)buf;
 while(n){
  ssize_t got=recv(fd,p,n,0);
  if(got>0){p+=(size_t)got;n-=(size_t)got;continue;}
  if(got<0&&errno==EINTR)continue;
  return -1;
 }
 return 0;
}
void c112_init(struct c112_receiver *r){
 if(!r)return;
 memset(r,0,sizeof(*r));alt111_video_init(&r->video);
}
void c112_end(struct c112_receiver *r){
 if(!r)return;
 alt111_video_destroy(&r->video);
 /* AES context is non-owning; owner must finalize it at teardown. */
 memset(r,0,sizeof(*r));alt111_video_init(&r->video);
}
int c112_begin(struct c112_receiver *r,uint64_t session,uint64_t conn,
               c112_decrypt_t cb,void *ctx){
 if(!r||!session||!conn||!cb||r->active)return C112_AUTH;
 r->session=session;r->connection=conn;r->decrypt=cb;r->crypto=ctx;
 r->stream=alt111_video_begin(&r->video,session);
 if(!r->stream){c112_end(r);return C112_INVALID;}
 r->active=1;return 0;
}
int c112_receive_one(struct c112_receiver *r,int fd){
 uint8_t h[128],*body=NULL;
 uint32_t n;unsigned op;
 int rc,result=C112_INVALID;
 if(!r||!r->active||fd<0)return C112_STALE;
 if(read_full(fd,h,128)){c112_end(r);return C112_CLOSED;}
 n=le32(h);op=h[4];
 if(n>ALT111_AU_LIMIT||(op==1&&n>ALT111_CODEC_CAP)){
  c112_end(r);return C112_INVALID;
 }
 if(n){
  body=(uint8_t*)malloc(n);
  if(!body){c112_end(r);return C112_INVALID;}
  if(read_full(fd,body,n)){free(body);c112_end(r);return C112_CLOSED;}
 }
 switch(op){
 case 1:
  if(!body||!n)break;
  rc=alt111_video_config(&r->video,r->stream,body,n);
  if(rc==ALT111_OK||rc==ALT111_RESTART_CONSUMER){
   ++r->configs;result=C112_CONFIG;
  }break;
 case 0:
  if(!body||!n||!r->decrypt||r->decrypt(r->crypto,body,n)){
   result=C112_AUTH;break;
  }
  rc=alt111_video_submit(&r->video,r->stream,body,n,1);
  if(rc==ALT111_OK){++r->frames;result=C112_FRAME;}
  else if(rc==ALT111_WAIT)result=C112_WAIT;
  break;
 case 2:case 3:case 4:case 5:result=C112_IGNORED;break;
 default:break;
 }
 free(body);
 if(result==C112_INVALID||result==C112_AUTH){
  c112_end(r); /* stale pictures cannot survive malformed packet/auth */
 }
 return result;
}
