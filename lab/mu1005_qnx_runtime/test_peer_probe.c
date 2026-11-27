/* Actual QNX AF_UNIX credential and roundtrip diagnostic, no OEM calls. */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int endpoint(const char *label,int fd){
 struct sockaddr_storage sa;
 socklen_t n=sizeof sa;uid_t u=(uid_t)-1;gid_t g=(gid_t)-1;
 memset(&sa,0,sizeof sa);errno=0;
 int a=getsockname(fd,(struct sockaddr*)&sa,&n),ae=errno,family=sa.ss_family;
 memset(&sa,0,sizeof sa);n=sizeof sa;errno=0;
 int b=getpeername(fd,(struct sockaddr*)&sa,&n),be=errno,peer_family=sa.ss_family;
 errno=0;int c=getpeereid(fd,&u,&g),ce=errno;
 printf("QNX_PEER %s local=%d/%d/family%d remote=%d/%d/family%d cred=%d/%d uid=%lu gid=%lu\n",label,a,ae,family,b,be,peer_family,c,ce,(unsigned long)u,(unsigned long)g);
 return a==0&&family==AF_UNIX&&c==0&&u==geteuid()&&g==getegid();
}
static int addr(struct sockaddr_un *a,const char *path){
 memset(a,0,sizeof *a);a->sun_family=AF_UNIX;
 if(strlen(path)>=sizeof a->sun_path)return -1;
 strcpy(a->sun_path,path);a->sun_len=(unsigned char)SUN_LEN(a);return 0;
}
static int run(int named){
 char dir[64],path[80],client_path[80];
 struct sockaddr_un a;
 int s=-1,c=-1,d=-1,ok=0;char v='Q',out=0;
 snprintf(dir,sizeof dir,"/tmp/r107-peer-%lu-%d",(unsigned long)getpid(),named);
 if(mkdir(dir,0700))return 0;
 snprintf(path,sizeof path,"%s/cprx.sock",dir);
 snprintf(client_path,sizeof client_path,"%s/client.sock",dir);
 s=socket(AF_UNIX,SOCK_STREAM,0);c=socket(AF_UNIX,SOCK_STREAM,0);
 if(s<0||c<0||addr(&a,path)||bind(s,(struct sockaddr*)&a,SUN_LEN(&a))||chmod(path,0600)||listen(s,2))goto done;
 if(named&&(addr(&a,client_path)||bind(c,(struct sockaddr*)&a,SUN_LEN(&a))||chmod(client_path,0600)))goto done;
 if(addr(&a,path)||connect(c,(struct sockaddr*)&a,SUN_LEN(&a)))goto done;
 d=accept(s,NULL,NULL);if(d<0)goto done;
 int aok=endpoint(named?"named-client":"anonymous-client",c);
 int bok=endpoint(named?"named-server":"anonymous-server",d);
 int r=send(c,&v,1,0)==1&&recv(d,&out,1,0)==1&&out==v;
 printf("QNX_PEER_ROUNDTRIP named=%d bytes=%d identity=%d/%d\n",named,r,aok,bok);
 ok=r&&aok&&bok;
done:
 if(!ok)printf("QNX_PEER_CASE_FAIL named=%d errno=%d\n",named,errno);
 if(d>=0)close(d);
 if(c>=0)close(c);
 if(s>=0)close(s);
 unlink(client_path);unlink(path);rmdir(dir);
 return ok;
}
int main(void){
 int anon=run(0),named=run(1);
 printf("QNX_PEER_SUMMARY anonymous=%d named=%d\n",anon,named);
 if(!anon)return 1;
 puts("QNX_PEER_PROBE_PASS");return 0;
}
