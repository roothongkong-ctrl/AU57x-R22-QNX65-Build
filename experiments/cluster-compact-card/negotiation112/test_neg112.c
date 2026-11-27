#include "neg112.h"
#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
static uint8_t key=0x4e;
static int crypt(void *ctx,uint8_t *p,size_t n){
 size_t j;uint8_t k=*(uint8_t*)ctx;
 for(j=0;j<n;++j){ p[j]^=k; }
 return 0;
}
static void sent(int fd,const uint8_t *p,size_t n){
 while(n){ssize_t x=send(fd,p,n,0);assert(x>0);p+=(size_t)x;n-=(size_t)x;}
}
static void packet(int fd,unsigned op,const uint8_t *b,size_t n,int encrypted){
 uint8_t h[128]={0},t[80];size_t j;assert(n<=sizeof(t));
 h[0]=(uint8_t)n;h[1]=(uint8_t)(n>>8);h[4]=(uint8_t)op;
 sent(fd,h,2);sent(fd,h+2,126);
 for(j=0;j<n;++j)t[j]=(uint8_t)(b[j]^(encrypted?key:0));
 if(n)sent(fd,t,n);
}
static int connect_local(unsigned port){
 int fd=socket(AF_INET,SOCK_STREAM,0);
 struct sockaddr_in a;assert(fd>=0);
 memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((uint16_t)port);
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(connect(fd,(struct sockaddr*)&a,sizeof(a))==0);
 return fd;
}
int main(void){
 const char *mapid="b7e6c5a0-2222-4000-8000-000000000002";
 const char *cardid="b7e6c5a0-2222-4000-8000-000000000003";
 const uint8_t avcc[]={1,0x64,0,0x1e,0xff,0xe1,0,4,0x67,0x64,0,0x1e,1,0,2,0x68,0xee};
 const uint8_t idr[]={0,0,0,3,0x65,0x88,0x84};
 struct n112_session s;struct n112_descriptor desc;struct n112_setup reply;
 struct n112_proof p={1,1,1,1,1,1},q;
 struct alt111_video map;uint64_t stream;
 int fd;unsigned i;
 n112_init(&s);alt111_video_init(&map);
 stream=alt111_video_begin(&map,70);assert(stream);
 assert(n112_offer(&s,&desc)==N112_NOT_READY);
 for(i=0;i<6;++i){
  q=p;
  if(i==0)q.explicit_opt_in=0;
  if(i==1)q.ios_112_observed=0;
  if(i==2)q.distinct_aes_context=0;
  if(i==3)q.codec_and_decoder_ready=0;
  if(i==4)q.overlay_alpha_ready=0;
  if(i==5)q.full_and_partial_teardown_verified=0;
  assert(n112_prepare(&s,&q,70,cardid,mapid,800,298,crypt,&key)==N112_NO_PROOF);
  assert(n112_offer(&s,&desc)==N112_NOT_READY);
 }
 assert(n112_prepare(&s,&p,70,mapid,mapid,800,298,crypt,&key)==N112_INVALID);
 assert(n112_prepare(&s,&p,70,cardid,mapid,800,298,crypt,&key)==N112_OK);
 assert(n112_offer(&s,&desc)==N112_OK);
 assert(desc.type==112 && desc.width==800&&desc.height==298);
 assert(!strcmp(desc.initial_url,"maps:/car/instrumentcluster/instructioncard"));
 assert(strcmp(desc.uuid,mapid)!=0);
 assert(n112_setup(&s,110,70,1109,&reply)==N112_NOT_READY);
 assert(n112_setup(&s,111,70,1119,&reply)==N112_NOT_READY);
 assert(n112_setup(&s,112,71,1129,&reply)==N112_STALE);
 assert(n112_setup(&s,112,70,1129,&reply)==N112_OK);
 assert(reply.type==112&&reply.data_port>0&&reply.stream_connection_id==1129);
 assert(n112_setup(&s,112,70,1129,&reply)==N112_DUPLICATE);
 fd=connect_local(reply.data_port);
 assert(n112_accept(&s)==N112_OK);
 assert(alt111_video_attach(&s.receiver.video)==ALT111_OK);
 packet(fd,1,avcc,sizeof(avcc),0);
 packet(fd,0,idr,sizeof(idr),1);
 assert(n112_receive_one(&s)==C112_CONFIG);
 assert(n112_receive_one(&s)==C112_FRAME);
 assert(map.source_aus==0&&map.count==0); /* original map untouched */
 assert(n112_teardown(&s,110,70,1109)==N112_NOT_READY);
 assert(n112_teardown(&s,112,70,1129)==N112_OK);
 assert(!s.receiver.video.count);
 close(fd);
 /* Test independent re-SETUP after a partial 112 tear down */
 for(i=0;i<300;++i){
  uint64_t cid=2000+i;
  assert(n112_setup(&s,112,70,cid,&reply)==N112_OK);
  fd=connect_local(reply.data_port);
  assert(n112_accept(&s)==N112_OK);
  assert(alt111_video_attach(&s.receiver.video)==ALT111_OK);
  packet(fd,1,avcc,sizeof(avcc),0);
  packet(fd,0,idr,sizeof(idr),1);
  assert(n112_receive_one(&s)==C112_CONFIG);
  assert(n112_receive_one(&s)==C112_FRAME);
  assert(n112_teardown(&s,112,70,cid)==N112_OK);
  close(fd);
  assert(s.receiver.video.count==0&&map.source_aus==0);
 }
 n112_destroy(&s);
 assert(alt111_video_end(&map,stream)==ALT111_OK);
 alt111_video_destroy(&map);
 puts("PASS: opt-in 112 descriptor, handshake response port, actual TCP H264 ingest, 110/111 unchanged, 300 partial teardowns");
}
