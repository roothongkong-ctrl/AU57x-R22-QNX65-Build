/* R15 experimental loopback record transport; not a vehicle installer.
 * Each record is one validated complete Annex-B AU, with session/sequence/time.
 * Sender never blocks media indefinitely: partial or failed writes close link.
 */
#ifndef R15_WIRE_H
#define R15_WIRE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <time.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#define R15_PORT 19822
#define R15_MAX_AU (2u*1024u*1024u+4096u)
#define R15_HEADER 36u
static inline void r15_put64(unsigned char *p,uint64_t x){
 unsigned i;for(i=0;i<8;i++)p[i]=(unsigned char)(x>>(8*i));
}
static inline uint64_t r15_get64(const unsigned char *p){
 unsigned i;uint64_t x=0;for(i=0;i<8;i++)x|=(uint64_t)p[i]<<(8*i);return x;
}
static inline void r15_put32(unsigned char *p,uint32_t x){
 unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(x>>(8*i));
}
static inline uint32_t r15_get32(const unsigned char *p){
 unsigned i;uint32_t x=0;for(i=0;i<4;i++)x|=(uint32_t)p[i]<<(8*i);return x;
}
static inline uint64_t r15_clock(void){
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
 return (uint64_t)t.tv_sec*1000000u+(uint64_t)t.tv_nsec/1000u;
}
static inline int r15_nonblock(int fd){
 int f=fcntl(fd,F_GETFL,0);return f>=0?fcntl(fd,F_SETFL,f|O_NONBLOCK):-1;
}
static inline int r15_connect_local(unsigned port){
 int fd=socket(AF_INET,SOCK_STREAM,0),err=0;
 struct sockaddr_in a;struct pollfd p;socklen_t n=sizeof(err);
 if(fd<0)return -1;
 if(r15_nonblock(fd)){close(fd);return -1;}
 memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((uint16_t)port);
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(connect(fd,(struct sockaddr*)&a,sizeof(a))==0)return fd;
 if(errno!=EINPROGRESS){close(fd);return -1;}
 p.fd=fd;p.events=POLLOUT;p.revents=0;
 if(poll(&p,1,5)!=1||getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&n)||err){
   close(fd);return -1;
 }
 return fd;
}
/* One bounded nonblocking attempt per segment. On partial send caller MUST
 * close the connection; receiver never accepts a truncated record. */
static inline int r15_send_record(int fd,uint64_t cid,uint64_t seq,uint64_t ts,
                                  unsigned idr,const unsigned char *au,size_t len){
 unsigned char h[R15_HEADER];ssize_t n;
 if(fd<0||!cid||!seq||!ts||!au||!len||len>R15_MAX_AU||idr>1)return -1;
 memcpy(h,"C112",4);r15_put32(h+4,(uint32_t)len);
 r15_put64(h+8,cid);r15_put64(h+16,seq);r15_put64(h+24,ts);r15_put32(h+32,idr);
 n=send(fd,h,sizeof(h),MSG_NOSIGNAL);if(n!=(ssize_t)sizeof(h))return -1;
 n=send(fd,au,len,MSG_NOSIGNAL);return n==(ssize_t)len?0:-1;
}
static inline int r15_read_exact(int fd,unsigned char *p,size_t n,int timeout_ms){
 uint64_t start=r15_clock();
 while(n){
   struct pollfd f;ssize_t got;uint64_t now=r15_clock();
   if(!start||now<start||now-start>=(uint64_t)timeout_ms*1000u)return -1;
   f.fd=fd;f.events=POLLIN;f.revents=0;
   int remain=timeout_ms-(int)((now-start)/1000u);
   if(poll(&f,1,remain)<=0)return -1;
   got=recv(fd,p,n,0);if(got<=0){if(got<0&&errno==EINTR)continue;return -1;}
   p+=(size_t)got;n-=(size_t)got;
 }
 return 0;
}
#endif
