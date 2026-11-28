/* Actual adapted donor executable, inherited endpoint, synthetic RF only. */
#define _POSIX_C_SOURCE 200809L
#include "supervised_notify.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
static struct stat expected;
static int ack_allowed;
static int ack_proof(void*p,const char*path){(void)p;(void)path;return ack_allowed;}
static int proof(void*p,const char*path){(void)p;struct stat x;return !stat(path,&x)&&S_ISREG(x.st_mode)&&x.st_dev==expected.st_dev&&x.st_ino==expected.st_ino&&x.st_size==expected.st_size;}
static void transfer(int f,unsigned char*b,size_t n,int send_it){
 while(n){struct pollfd p={f,send_it?POLLOUT:POLLIN,0};assert(poll(&p,1,3000)==1);
  ssize_t k=send_it?send(f,b,n,0):recv(f,b,n,0);
  if(k<0&&(errno==EINTR||errno==EAGAIN))continue;
  assert(k>0);b+=k;n-=(size_t)k;}
}
static void response(int f){unsigned char h[8],b[8192];transfer(f,h,8,0);assert(h[0]==2&&h[1]==0&&h[4]==1);size_t n=h[2]+256u*h[3];assert(n&&n<=sizeof b);transfer(f,b,n,0);}
int main(int argc,char**argv){
 assert(argc==2&&stat(argv[1],&expected)==0&&setenv("MU1005_BRIDGE_FD","3",1)==0);
 MuDaemonSupervisor s={.fd=-1};
 Au98Handoff handoff={0};
 int offered=-1,second=-1;
 MuBtNotifyBinding binding={.handoff=&handoff,.prepared_fd=&offered};
 assert(mu_supervised_notify_prepare(&s,&binding,argv[1],environ,proof,NULL,ack_proof,NULL)==-1);
 assert(offered==-1&&!binding.daemon_verified&&s.pid<=0);
 ack_allowed=1;
 assert(mu_supervised_notify_prepare(&s,&binding,argv[1],environ,proof,NULL,ack_proof,NULL)==0);
 assert(offered==s.fd&&binding.worker==NULL&&binding.daemon_context==&s);
 assert(binding.daemon_verified(binding.daemon_context,offered)==1);
 assert(binding.ack_daemon_verified(binding.daemon_context,offered)==1);
 assert(mu_supervised_notify_prepare(&s,&binding,argv[1],environ,proof,NULL,ack_proof,NULL)==-1);
 assert(mu_daemon_supervisor_live(&s,s.fd)==1);
 assert(mu_daemon_supervisor_offer(&s,&second)==-1&&second==-1);
 assert(mu_daemon_supervisor_witness(&s,offered)==1);
 assert(mu_daemon_supervisor_close(&s)==1&&fcntl(offered,F_GETFD)>=0);
 int other=dup(s.fd);assert(other>=0&&!mu_daemon_supervisor_live(&s,other));close(other);
 unsigned char frames[]={1,0,6,0,1,0,0,0,2,1,2,3,4,5,2,0,6,0,1,0,0,0,255,85,2,0,238,16};
 unsigned char ack[]={4,0,0,0,1,0,0,0};
 transfer(s.fd,frames,sizeof frames,1);response(s.fd);assert(mu_daemon_supervisor_live(&s,s.fd)==1);
 transfer(s.fd,ack,sizeof ack,1);response(s.fd);
 assert(mu_daemon_supervisor_live(&s,s.fd)==1);
 assert(close(offered)==0); /* owner-side close, not supervisor */
 int rc=mu_daemon_supervisor_close(&s);
 for(unsigned i=0;i<100&&rc==1;i++){struct timespec t={0,50000000};nanosleep(&t,NULL);rc=mu_daemon_supervisor_close(&s);}
 assert(rc==0&&s.reaped&&!mu_daemon_supervisor_live(&s,s.fd));
 assert(!binding.daemon_verified(binding.daemon_context,offered));
 puts("MU1005_SUPERVISOR_ACTUAL_DONOR_FD_PAIR_AND_REAP_PASS_NO_OEM");
 puts("MU1005_SUPERVISED_NOTIFY_EXACT_ACK_HANDOFF_PASS_NO_OEM");
}
