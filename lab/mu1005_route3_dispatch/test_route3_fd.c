/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "route3_fd.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
static int proof(void*v,int fd,const uint8_t*m,uint32_t g){
 return *(int*)v==fd&&(!m|| (m[0]==1&&g==7));
}
static void all(int fd,uint8_t*b,size_t n,int write_it){
 while(n){ssize_t r=write_it?write(fd,b,n):read(fd,b,n);assert(r>0);b+=r;n-=(size_t)r;}
}
static void run(int mode){
 int p[2],status;assert(socketpair(AF_UNIX,SOCK_STREAM,0,p)==0);
 pid_t child=fork();assert(child>=0);
 if(!child){
  close(p[0]);uint8_t open[14]={1,0,6,0,7,0,0,0,1,2,3,4,5,6};
  all(p[1],open,14,1);
  uint8_t tx[11];all(p[1],tx,11,0);assert(tx[0]==2&&tx[4]==7&&!memcmp(tx+8,"abc",3));
  if(mode==0){uint8_t reply[19]={4,0,0,0,7,0,0,0,2,0,3,0,7,0,0,0,'x','y','z'};all(p[1],reply,3,1);all(p[1],reply+3,16,1);}
  if(mode==1){uint8_t reply[8]={4,0,0,0,8,0,0,0};all(p[1],reply,8,1);}
  close(p[1]);_exit(0);
 }
 close(p[1]);int flags=fcntl(p[0],F_GETFL,0);assert(fcntl(p[0],F_SETFL,flags|O_NONBLOCK)==0);
 Route3Fd s={0};assert(route3_fd_attach(&s,p[0],&p[0],proof)==0);
 uint64_t d=route3_fd_now(NULL)+3000;
 while(!s.link.connected)assert(route3_fd_step(&s,d)==1);
 int rc=route3_fd_send(&s,(const uint8_t*)"abc",3,d);
 if(mode==0){assert(rc==3);uint8_t rx[3];int got=0;while(!got){got=route3_fd_recv(&s,rx,3);if(!got)assert(route3_fd_step(&s,d)==1);}
 assert(got==3&&!memcmp(rx,"xyz",3));}
 else assert(rc==-1&&s.link.poisoned);
 route3_fd_detach(&s);close(p[0]);assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
}
int main(void){run(0);run(1);run(2);puts("REAL_FD_RAW_TRANSPORT_ACK_RX_LOSS_PASS_NO_OEM");}
