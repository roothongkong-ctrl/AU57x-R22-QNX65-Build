/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "daemon_supervisor.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#define PORT 57342u
static int cloexec(int f){int fl=fcntl(f,F_GETFD);return fl>=0&&fcntl(f,F_SETFD,fl|FD_CLOEXEC)==0;}
static int addr(int f,int peer,struct sockaddr_in*a){
 socklen_t n=sizeof *a;memset(a,0,sizeof *a);
 return (peer?getpeername(f,(struct sockaddr*)a,&n):getsockname(f,(struct sockaddr*)a,&n))==0&&n==sizeof *a&&a->sin_family==AF_INET&&a->sin_addr.s_addr==htonl(INADDR_LOOPBACK);
}
static int same(const struct sockaddr_in*a,const struct sockaddr_in*b){return a->sin_family==b->sin_family&&a->sin_port==b->sin_port&&a->sin_addr.s_addr==b->sin_addr.s_addr;}
static int exact(MuDaemonSupervisor*s,int fd){
 struct stat st;struct sockaddr_in a,b;int fl;
 return fd==s->fd&&fd>=0&&(fl=fcntl(fd,F_GETFD))>=0&&(fl&FD_CLOEXEC)&&!fstat(fd,&st)&&S_ISSOCK(st.st_mode)&&
 st.st_dev==s->identity.st_dev&&st.st_ino==s->identity.st_ino&&addr(fd,0,&a)&&addr(fd,1,&b)&&same(&a,&s->local)&&same(&b,&s->peer);
}
static int child_live(MuDaemonSupervisor*s){
 int status;pid_t got=waitpid(s->pid,&status,WNOHANG);
 if(got==0)return 1;
 if(got==s->pid||(got<0&&errno==ECHILD))s->reaped=1;
 return 0;
}
int mu_daemon_supervisor_live(MuDaemonSupervisor*s,int fd){
 if(!s||!s->active||s->reaped||s->pid<=0||!exact(s,fd))return 0;
 if(!child_live(s)){s->active=0;return 0;}
 struct pollfd p={fd,POLLIN,0};int n=poll(&p,1,0);
 if(n<0||p.revents&(POLLERR|POLLHUP|POLLNVAL)){s->active=0;return 0;}
 if(p.revents&POLLIN){unsigned char v;ssize_t k=recv(fd,&v,1,MSG_PEEK|MSG_DONTWAIT);
  if(k==0||(k<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK)){s->active=0;return 0;}}
 return 1;
}
int mu_daemon_supervisor_witness(void*v,int fd){
 MuDaemonSupervisor*s=v;
 return s&&s->transferred&&mu_daemon_supervisor_live(s,fd)==1;
}
int mu_daemon_supervisor_offer(MuDaemonSupervisor*s,int*fd){
 if(!s||!fd||*fd>=0||s->transferred||
    mu_daemon_supervisor_live(s,s->fd)!=1)return -1;
 s->transferred=1;*fd=s->fd;return 0;
}
/* Optional protocol version flag. Production notify requires this mode;
 * old standalone laboratory tests remain compatible. */
static int ready_mode(char*const env[]){
 unsigned count=0;
 if(!env)return -1;
 for(unsigned i=0;env[i];i++){
  if(i>=127)return -1;
  if(!strncmp(env[i],"MU1005_BRIDGE_READY=",20)){
   if(strcmp(env[i],"MU1005_BRIDGE_READY=1")||++count!=1)return -1;
  }
 }
 return (int)count;
}
static int await_ready(MuDaemonSupervisor*s){
 static const unsigned char marker[]="MU1005-READY-v1\n";
 unsigned char b[sizeof marker-1];size_t n=0;
 struct timespec begin,now;
 if(clock_gettime(CLOCK_MONOTONIC,&begin))return -1;
 while(n<sizeof b){
  if(clock_gettime(CLOCK_MONOTONIC,&now))return -1;
  long ms=(long)(now.tv_sec-begin.tv_sec)*1000L+(now.tv_nsec-begin.tv_nsec)/1000000L;
  if(ms<0||ms>=15000||!mu_daemon_supervisor_live(s,s->fd))return -1;
  struct pollfd p={s->fd,POLLIN,0};
  int rc=poll(&p,1,100);
  if(rc<0&&errno==EINTR)continue;
  if(rc<0||p.revents&(POLLERR|POLLHUP|POLLNVAL))return -1;
  if(!rc)continue;
  ssize_t got=recv(s->fd,b+n,sizeof b-n,MSG_DONTWAIT);
  if(got<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))continue;
  if(got<=0)return -1;
  n+=(size_t)got;
  if(memcmp(b,marker,n))return -1;
 }
 return mu_daemon_supervisor_live(s,s->fd)?0:-1;
}
static int env_ok(char*const env[]){
 unsigned count=0,found=0;
 if(!env)return 0;
 for(;env[count];count++){if(count>=127)return 0;if(!strncmp(env[count],"MU1005_BRIDGE_FD=",17)){if(strcmp(env[count],"MU1005_BRIDGE_FD=3"))return 0;found++;}}
 return found==1&&ready_mode(env)>=0;
}
static pid_t launch(const char*path,int accepted,char*const env[]){
 char *argv[]={(char*)path,"--mu1005-ack-fd",NULL};
#if defined(__QNXNTO__)
 int map[]={0,1,2,accepted};
 return spawn(path,4,map,NULL,argv,env);
#else
 posix_spawn_file_actions_t a;pid_t pid=-1;
 if(posix_spawn_file_actions_init(&a)!=0)return -1;
 int r=posix_spawn_file_actions_adddup2(&a,accepted,3);
 if(!r&&accepted!=3)r=posix_spawn_file_actions_addclose(&a,accepted);
 if(!r)r=posix_spawn(&pid,path,&a,NULL,argv,env);
 posix_spawn_file_actions_destroy(&a);
 return r?-1:pid;
#endif
}
int mu_daemon_supervisor_start(MuDaemonSupervisor*s,const char*path,char*const env[],int(*verify)(void*,const char*),void*ctx){
 int l=-1,c=-1,a=-1,fl,one=1,err=0;pid_t pid=-1;
 struct sockaddr_in bindaddr={0},local,remote,al,ar; socklen_t n=sizeof err;
 if(!s||s->active||s->pid>0||!path||path[0]!='/'||!verify||!env_ok(env)||verify(ctx,path)!=1)return -1;
 memset(s,0,sizeof *s);s->fd=-1;
 l=socket(AF_INET,SOCK_STREAM,0);if(l<0||!cloexec(l)||setsockopt(l,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one))goto bad;
 #if defined(__QNXNTO__)
 bindaddr.sin_len=sizeof bindaddr;
#endif
 bindaddr.sin_family=AF_INET;bindaddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);bindaddr.sin_port=htons(PORT);
 if(bind(l,(struct sockaddr*)&bindaddr,sizeof bindaddr)||listen(l,1))goto bad;
 c=socket(AF_INET,SOCK_STREAM,0);if(c<0||!cloexec(c)||(fl=fcntl(c,F_GETFL,0))<0||fcntl(c,F_SETFL,fl|O_NONBLOCK))goto bad;
 if(connect(c,(struct sockaddr*)&bindaddr,sizeof bindaddr)<0){
  if(errno!=EINPROGRESS&&errno!=EWOULDBLOCK)goto bad;
  struct pollfd p={c,POLLOUT,0};
  if(poll(&p,1,1000)!=1||!(p.revents&POLLOUT)||p.revents&(POLLERR|POLLHUP|POLLNVAL)||getsockopt(c,SOL_SOCKET,SO_ERROR,&err,&n)||n!=sizeof err||err)goto bad;
 }
 if(!addr(c,0,&local)||!addr(c,1,&remote)||remote.sin_port!=htons(PORT))goto bad;
 fl=fcntl(l,F_GETFL,0);if(fl<0||fcntl(l,F_SETFL,fl|O_NONBLOCK))goto bad;
 a=accept(l,NULL,NULL);if(a<0||!cloexec(a)||!addr(a,0,&al)||!addr(a,1,&ar)||!same(&local,&ar)||!same(&remote,&al))goto bad;
 /* Keep accepted distinct from FD 3 in either spawn implementation. */
 if(a==3){int d=fcntl(a,F_DUPFD,4);if(d<0)goto bad;close(a);a=d;if(!cloexec(a))goto bad;}
 fl=fcntl(a,F_GETFL,0);if(fl<0||fcntl(a,F_SETFL,fl&~O_NONBLOCK))goto bad;
 close(l);l=-1;
 if(fstat(c,&s->identity)||!S_ISSOCK(s->identity.st_mode)||verify(ctx,path)!=1)goto bad;
 pid=launch(path,a,env);if(pid<=0)goto bad;
 close(a);s->fd=c;s->pid=pid;s->local=local;s->peer=remote;s->active=1;
 if(ready_mode(env)==1&&await_ready(s)!=0){
  /* No ownership transfer on failure. Retain PID until certified reap. */
  (void)mu_daemon_supervisor_close(s);return -1;
 }
 return 0;
bad:
 if(a>=0)close(a);
 if(c>=0)close(c);
 if(l>=0)close(l);
 return -1;
}
int mu_daemon_supervisor_close(MuDaemonSupervisor*s){
 if(!s||s->pid<=0)return -1;
 /* Never close a reused descriptor, and never kill a PID after reaping. */
 if(s->fd>=0){
  if(s->transferred){
   if(exact(s,s->fd))return 1; /* owner's FD: never close it ourselves */
   s->fd=-1;
  }else{
   if(!exact(s,s->fd))return -1;
   close(s->fd);s->fd=-1;
  }
 }
 s->active=0;
 if(s->reaped)return 0;
 return child_live(s)?1:(s->reaped?0:-1);
}
