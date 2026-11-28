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
static int env_ok(char*const env[]){
 unsigned count=0,found=0;
 if(!env)return 0;
 for(;env[count];count++){if(count>=127)return 0;if(!strncmp(env[count],"MU1005_BRIDGE_FD=",17)){if(strcmp(env[count],"MU1005_BRIDGE_FD=3"))return 0;found++;}}
 return found==1;
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
 if(s->fd>=0){if(!exact(s,s->fd))return -1;close(s->fd);s->fd=-1;}
 s->active=0;
 if(s->reaped)return 0;
 return child_live(s)?1:(s->reaped?0:-1);
}
