/* Offline host fixture, no native owner, no MFi, no vehicle. */
#define _POSIX_C_SOURCE 200809L
#include "owner_bridge.h"
#include "daemon_client.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
static const uint8_t peer[6]={1,2,3,4,5,6};
static const uint8_t marker[6]={0xff,0x55,2,0,0xee,0x10};
struct fixture{int allowed;uint32_t token;const uint8_t *held;size_t held_n,n;uint8_t bytes[256];};
static int req(void *v,uint8_t scn,uint16_t cap,uint32_t g){(void)v;return scn==3&&cap==32&&g?2:-1;}
static void cancel(void *v,uint32_t g){(void)v;assert(g);}
static int allow(void*v,uint32_t g,const uint8_t p[6]){return ((struct fixture*)v)->allowed&&g==1&&!memcmp(p,peer,6);}
static int copy(void*v,uint32_t token,uint8_t*p,size_t n){(void)v;if(token!=99||n!=6)return -1;memcpy(p,marker,6);return 0;}
static int send_frame(void*v,const uint8_t*p,size_t n,uint32_t*t){struct fixture*f=v;assert(!f->held&&n&&n<=23&&f->n+n<=sizeof f->bytes);f->held=p;f->held_n=n;memcpy(f->bytes+f->n,p,n);f->n+=n;*t=++f->token;return 0;}
static int daemon(void *v,int fd){(void)v;struct sockaddr_in a;socklen_t n=sizeof a;return getpeername(fd,(struct sockaddr*)&a,&n)==0&&a.sin_family==AF_INET&&a.sin_addr.s_addr==htonl(INADDR_LOOPBACK)&&a.sin_port==htons(57342);}
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void evt(uint8_t e[16],uint8_t type,uint16_t n,uint32_t token){memset(e,0,16);e[0]=type;e[8]=(uint8_t)n;e[9]=(uint8_t)(n>>8);for(unsigned i=0;i<4;i++)e[12+i]=(uint8_t)(token>>(8*i));}
int main(void){
 struct fixture f={.allowed=1};struct au_owner_bridge b;AuDaemonClient c;
 struct au_owner_ops ops={&f,req,cancel,allow,copy,send_frame};
 struct au_admission a={1,1,1,1,3,32};
 AuDaemonAdmission guard={&f,allow,daemon};
 assert(!aub_init(&b,&ops));assert(!aub_begin(&b,&a,peer));assert(!aub_connected(&b,1,peer,23));
 au_daemon_client_init(&c,&b.rf,guard);assert(!au_daemon_client_start(&c,now()));
 uint64_t start=now();int replied=0;uint8_t e[16];
 while(now()-start<5000){
  assert(au_daemon_client_step(&c,now())>=0);
  if(replied&&f.n>6&&b.rf.tx_busy){
   assert(f.bytes[6]==0xff&&f.bytes[7]==0x5a);
   f.allowed=0;
   assert(au_daemon_client_step(&c,now())<0&&c.fd<0&&b.rf.tx_busy);
   assert(!aub_terminal(&b,1)&&b.session.state==AU_DRAINING);
   assert(f.held&&f.held_n);
   evt(e,6,0,f.token);f.held=NULL;
   assert(aub_event(&b,1,e,16)<0&&!b.rf.tx_busy&&b.session.state==AU_IDLE);
   puts("AU57X_OWNER_CALLBACK_RUST_SOCKET_MARKER_SYN_REVOKE_OK_NO_OEM");
   /* Retain upstream fixture acceptance marker. */
   puts("AU57X_DIRECT_DAEMON_MARKER_SYN_WIRED_REVOKE_OK_NO_OEM");return 0;
  }
  if(b.rf.tx_busy){assert(f.held);evt(e,6,0,f.token);f.held=NULL;assert(!aub_event(&b,1,e,16));}
  if(!replied&&f.n==6&&!b.rf.tx_busy){assert(!memcmp(f.bytes,marker,6));evt(e,5,6,99);assert(!aub_event(&b,1,e,16));replied=1;}
  struct timespec delay={0,1000000};nanosleep(&delay,0);
 }
 au_daemon_client_stop(&c);aub_stop(&b);return 2;
}
