/* SPDX-License-Identifier: GPL-3.0-only
 * Private UNIX transport, no OEM operations. Kernel credentials alone
 * do not prove the original SI/DIO owner; server must still verify it. */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
static int same(const struct stat*a,const struct stat*b){
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_uid==b->st_uid&&
 a->st_gid==b->st_gid&&a->st_mode==b->st_mode;
}
static int check(const char*path,const char*dir,uid_t uid,gid_t gid,struct stat*s,struct stat*d){
 return !lstat(dir,d)&&S_ISDIR(d->st_mode)&&(d->st_mode&07777)==0700&&
 d->st_uid==uid&&d->st_gid==gid&&!lstat(path,s)&&S_ISSOCK(s->st_mode)&&
 (s->st_mode&07777)==0600&&s->st_uid==uid&&s->st_gid==gid;
}
int mu1005_local_connect(const char*path,uint32_t uid,uint32_t gid,unsigned ms){
 struct sockaddr_un a;struct stat s,d,s2,d2;char dir[sizeof a.sun_path];
 size_t n;int fd=-1,fl,error=0;uid_t u;gid_t g;
 if(!path||path[0]!='/'||!ms||ms>8000||uid!=(uid_t)uid||gid!=(gid_t)gid)return -EINVAL;
 n=strlen(path);
 if(n>=sizeof a.sun_path||n<4||strstr(path,"//")||strstr(path,"/../")||strstr(path,"/./"))return -EINVAL;
 const char*slash=strrchr(path,'/');
 if(slash==path||!slash[1]||!strcmp(slash+1,".")||!strcmp(slash+1,".."))return -EINVAL;
 memcpy(dir,path,(size_t)(slash-path));dir[slash-path]=0;
 if(!check(path,dir,(uid_t)uid,(gid_t)gid,&s,&d))return -EACCES;
 fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return -errno;
 fl=fcntl(fd,F_GETFL,0);
 if(fl<0||fcntl(fd,F_SETFL,fl|O_NONBLOCK)<0||fcntl(fd,F_SETFD,FD_CLOEXEC)<0)goto bad;
 memset(&a,0,sizeof a);a.sun_family=AF_UNIX;memcpy(a.sun_path,path,n+1);
#ifdef __QNXNTO__
 a.sun_len=(unsigned char)SUN_LEN(&a);
 socklen_t len=(socklen_t)SUN_LEN(&a);
#else
 socklen_t len=(socklen_t)(offsetof(struct sockaddr_un,sun_path)+n+1);
#endif
 if(connect(fd,(struct sockaddr*)&a,len)<0){
  if(errno!=EINPROGRESS&&errno!=EWOULDBLOCK)goto bad;
  struct pollfd p={fd,POLLOUT,0};int rc=poll(&p,1,(int)ms);
  socklen_t z=sizeof error;
  if(rc!=1||!(p.revents&POLLOUT)||p.revents&(POLLERR|POLLHUP|POLLNVAL)||
     getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&z)||error){errno=error?error:ETIMEDOUT;goto bad;}
 }
#ifdef __QNXNTO__
 if(getpeereid(fd,&u,&g))goto bad;
#elif defined(__linux__)
 struct ucred cred;socklen_t z=sizeof cred;
 if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&cred,&z)||z!=sizeof cred)goto bad;
 u=cred.uid;g=cred.gid;
#else
 errno=ENOTSUP;goto bad;
#endif
 if(u!=(uid_t)uid||g!=(gid_t)gid||!check(path,dir,u,g,&s2,&d2)||!same(&s,&s2)||!same(&d,&d2)){errno=EACCES;goto bad;}
 if(fcntl(fd,F_SETFL,fl)<0)goto bad;
 return fd;
bad:
 error=errno?errno:EIO;close(fd);return -error;
}
