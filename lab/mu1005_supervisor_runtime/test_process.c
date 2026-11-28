/* SPDX-License-Identifier: GPL-3.0-only */
/* Real process, real TCP pair and FD inheritance, no RF or OEM claims. */
#define _POSIX_C_SOURCE 200809L
#include "daemon_supervisor.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static struct stat expected;
static int verify(void *ctx,const char *path){
 struct stat st;(void)ctx;
 return stat(path,&st)==0&&S_ISREG(st.st_mode)&&st.st_dev==expected.st_dev&&
        st.st_ino==expected.st_ino&&st.st_size==expected.st_size;
}
static int transfer(int fd,unsigned char *p,size_t n,int write_it){
 while(n){
  struct pollfd wait={fd,write_it?POLLOUT:POLLIN,0};
  if(poll(&wait,1,3000)!=1)return -1;
  ssize_t got=write_it?send(fd,p,n,0):recv(fd,p,n,0);
  if(got<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
  if(got<=0)return -1;
  p+=got;n-=(size_t)got;
 }
 return 0;
}
int main(int argc,char **argv){
 unsigned char msg[4]={'M','U','1','0'};
 if(argc==2&&!strcmp(argv[1],"--mu1005-ack-fd")){
  unsigned char in[4];
  assert(getenv("MU1005_BRIDGE_FD")&&!strcmp(getenv("MU1005_BRIDGE_FD"),"3"));
  assert(transfer(3,in,sizeof in,0)==0&&!memcmp(in,msg,sizeof in));
  in[3]='5';assert(transfer(3,in,sizeof in,1)==0);
  /* Remain alive until the original owner closes, then exit. */
  struct pollfd p={3,POLLIN,0};
  assert(poll(&p,1,5000)==1&&recv(3,in,1,0)==0);
  puts("SUPERVISOR_CHILD_REAL_FD3_EOF_PASS");
  return 0;
 }
#if defined(__QNXNTO__)
 const char *path="/proc/boot/mu-child";
#else
 const char *path=argv[0];
#endif
 if(path[0]!='/'||stat(path,&expected)!=0){
  fprintf(stderr,"SUPERVISOR_CHILD_STAT_FAILED path=%s errno=%d (%s)\n",path,errno,strerror(errno));
  return 11;
 }
 printf("SUPERVISOR_CHILD_STAT_OK mode=%lo size=%ld\n",(unsigned long)expected.st_mode,(long)expected.st_size);
 char *env[]={"MU1005_BRIDGE_FD=3","PATH=/proc/boot:/bin","LD_LIBRARY_PATH=/proc/boot",NULL};
 MuDaemonSupervisor s={.fd=-1};int fd=-1,second=-1;
 assert(mu_daemon_supervisor_start(&s,path,env,verify,NULL)==0);
 assert(mu_daemon_supervisor_offer(&s,&fd)==0);
 assert(mu_daemon_supervisor_offer(&s,&second)==-1&&second==-1);
 assert(mu_daemon_supervisor_witness(&s,fd)==1);
 assert(transfer(fd,msg,sizeof msg,1)==0);
 assert(transfer(fd,msg,sizeof msg,0)==0&&!memcmp(msg,"MU15",4));
 assert(mu_daemon_supervisor_close(&s)==1&&fcntl(fd,F_GETFD)>=0);
 assert(close(fd)==0);
 int rc=mu_daemon_supervisor_close(&s);
 for(int i=0;i<100&&rc==1;i++){
  struct timespec delay={0,50000000};nanosleep(&delay,NULL);rc=mu_daemon_supervisor_close(&s);
 }
 assert(rc==0&&s.reaped&&!mu_daemon_supervisor_witness(&s,fd));
 puts("SUPERVISOR_REAL_SPAWN_FD3_REAP_PASS_NO_OEM");
 return 0;
}
