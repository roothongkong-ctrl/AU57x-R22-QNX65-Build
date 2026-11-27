#include "c112_ingest.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
static uint8_t key=0x5a;
static int xor_crypto(void *ctx,uint8_t *p,size_t n){
 size_t i;uint8_t k=*(uint8_t*)ctx;
 for(i=0;i<n;i++)p[i]^=k;
 return 0;
}
static int fail_crypto(void *ctx,uint8_t *p,size_t n){
 (void)ctx;(void)p;(void)n;return -1;
}
static void sendall(int fd,const uint8_t *p,size_t n){
 while(n){ssize_t z=send(fd,p,n,0);assert(z>0);p+=(size_t)z;n-=(size_t)z;}
}
static void sendpkt(int fd,unsigned op,const uint8_t *data,size_t n,int encrypt){
 uint8_t hdr[128]={0},body[256];size_t i;
 assert(n<=sizeof(body));
 hdr[0]=(uint8_t)n;hdr[1]=(uint8_t)(n>>8);hdr[2]=(uint8_t)(n>>16);hdr[3]=(uint8_t)(n>>24);hdr[4]=(uint8_t)op;
 sendall(fd,hdr,1);sendall(fd,hdr+1,7);sendall(fd,hdr+8,120);
 for(i=0;i<n;i++)body[i]=(uint8_t)(data[i]^(encrypt?key:0));
 if(n)sendall(fd,body,n);
}
static void tcp_loopback_test(void){
 const uint8_t cfg[]={1,0x64,0,0x1e,0xff,0xe1,0,4,0x67,0x64,0,0x1e,1,0,2,0x68,0xee};
 const uint8_t idr[]={0,0,0,3,0x65,0x88,0x84};
 int listen_fd=socket(AF_INET,SOCK_STREAM,0),client,server;
 struct sockaddr_in a; socklen_t alen=sizeof(a);
 struct c112_receiver rcv;
 uint8_t bad[128]={0};
 assert(listen_fd>=0);
 memset(&a,0,sizeof(a));a.sin_family=AF_INET;
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=0;
 assert(bind(listen_fd,(struct sockaddr*)&a,sizeof(a))==0);
 assert(listen(listen_fd,1)==0);
 assert(getsockname(listen_fd,(struct sockaddr*)&a,&alen)==0);
 client=socket(AF_INET,SOCK_STREAM,0);assert(client>=0);
 assert(connect(client,(struct sockaddr*)&a,sizeof(a))==0);
 server=accept(listen_fd,NULL,NULL);assert(server>=0);
 c112_init(&rcv);
 assert(c112_begin(&rcv,99,199,xor_crypto,&key)==0);
 assert(alt111_video_attach(&rcv.video)==ALT111_OK);
 sendpkt(client,1,cfg,sizeof(cfg),0);
 sendpkt(client,0,idr,sizeof(idr),1);
 assert(c112_receive_one(&rcv,server)==C112_CONFIG);
 assert(c112_receive_one(&rcv,server)==C112_FRAME);
 assert(rcv.video.count>0);
 c112_end(&rcv);
 /* Fuzz input framing: size=UINT32_MAX is rejected before allocation. */
 assert(c112_begin(&rcv,100,200,xor_crypto,&key)==0);
 bad[0]=bad[1]=bad[2]=bad[3]=255;bad[4]=0;
 sendall(client,bad,128);
 assert(c112_receive_one(&rcv,server)==C112_INVALID);
 assert(!rcv.active&&rcv.video.count==0);
 /* Truncation across network must invalidate private stream state. */
 assert(c112_begin(&rcv,101,201,xor_crypto,&key)==0);
 memset(bad,0,sizeof(bad));bad[0]=10;bad[4]=0;
 sendall(client,bad,128);sendall(client,idr,3);
 assert(shutdown(client,SHUT_WR)==0);
 assert(c112_receive_one(&rcv,server)==C112_CLOSED);
 assert(!rcv.active&&rcv.video.count==0);
 c112_end(&rcv);
 close(server);close(client);close(listen_fd);
 puts("PASS AF_INET TCP loopback with encrypted H264 frame, oversized packet rejection and truncation teardown");
}

int main(void){
 tcp_loopback_test();
 const uint8_t avcc[]={1,0x64,0,0x1e,0xff,0xe1,0,4,0x67,0x64,0,0x1e,1,0,2,0x68,0xee};
 const uint8_t idr[]={0,0,0,3,0x65,0x88,0x84};
 struct c112_receiver card;
 struct alt111_video original;
 struct alt111_output_ticket ticket;
 uint64_t stream;
 const uint8_t *bytes=NULL;
 size_t count=0;
 int fd[2];unsigned j;
 c112_init(&card);alt111_video_init(&original);
 stream=alt111_video_begin(&original,99);assert(stream>0);
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
 assert(c112_begin(&card,100,200,NULL,NULL)==C112_AUTH);
 assert(c112_begin(&card,100,200,xor_crypto,&key)==0);
 assert(alt111_video_attach(&card.video)==ALT111_OK);
 sendpkt(fd[1],1,avcc,sizeof(avcc),0);
 assert(c112_receive_one(&card,fd[0])==C112_CONFIG);
 sendpkt(fd[1],0,idr,sizeof(idr),1);
 assert(c112_receive_one(&card,fd[0])==C112_FRAME);
 assert(alt111_video_peek(&card.video,&bytes,&count,&ticket)==ALT111_OK);
 assert(bytes&&count>sizeof(idr));
 assert(alt111_video_advance(&card.video,&ticket,count)==ALT111_OK);
 assert(original.count==0 && original.source_aus==0);
 sendpkt(fd[1],2,NULL,0,0);
 assert(c112_receive_one(&card,fd[0])==C112_IGNORED);
 sendpkt(fd[1],0,idr,sizeof(idr),1);
 assert(c112_receive_one(&card,fd[0])==C112_FRAME);
 assert(card.video.count>0);
 sendpkt(fd[1],9,NULL,0,0);
 assert(c112_receive_one(&card,fd[0])==C112_INVALID);
 assert(!card.active&&card.video.count==0);
 close(fd[0]);close(fd[1]);
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
 assert(c112_begin(&card,102,202,fail_crypto,&key)==0);
 assert(alt111_video_attach(&card.video)==ALT111_OK);
 sendpkt(fd[1],1,avcc,sizeof(avcc),0);
 assert(c112_receive_one(&card,fd[0])==C112_CONFIG);
 sendpkt(fd[1],0,idr,sizeof(idr),1);
 assert(c112_receive_one(&card,fd[0])==C112_AUTH);
 assert(!card.active);
 close(fd[0]);close(fd[1]);
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
 assert(c112_begin(&card,103,203,xor_crypto,&key)==0);
 close(fd[1]);
 assert(c112_receive_one(&card,fd[0])==C112_CLOSED);
 assert(!card.active);close(fd[0]);
 for(j=0;j<1000;j++){
  assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
  assert(c112_begin(&card,1000+j,2000+j,xor_crypto,&key)==0);
  assert(alt111_video_attach(&card.video)==ALT111_OK);
  sendpkt(fd[1],1,avcc,sizeof(avcc),0);
  assert(c112_receive_one(&card,fd[0])==C112_CONFIG);
  sendpkt(fd[1],0,idr,sizeof(idr),1);
  assert(c112_receive_one(&card,fd[0])==C112_FRAME);
  c112_end(&card);
  assert(!card.video.count&&original.source_aus==0);
  close(fd[0]);close(fd[1]);
 }
 assert(alt111_video_end(&original,stream)==ALT111_OK);
 alt111_video_destroy(&original);c112_end(&card);
 puts("PASS 128-byte packet socket read, H264 avcC/IDR validation, per-instance crypto, 111 isolation, 1000 reconnects");
 return 0;
}
