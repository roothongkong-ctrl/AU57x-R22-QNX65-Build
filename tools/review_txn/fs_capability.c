#define _POSIX_C_SOURCE 200809L
/* QNX emulator filesystem capability probe. Only creates a unique disposable
 * child directory in a caller-created lab directory containing LAB_ONLY.
 * No mount/remount, OEM path, services, or installation. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CAP 4096
static int pcat(char *out,const char *d,const char *n){int q=snprintf(out,CAP,"%s/%s",d,n);return q<0||q>=CAP?-1:0;}
/* QNX 6.5 headers here do not declare mkdtemp; create the disposable name
 * ourselves so the same source compiles for host tests and ARM QNX. */
static int make_lab_dir(char *d){
 static const char alphabet[]="abcdefghijklmnopqrstuvwxyz0123456789";
 unsigned long seed=(unsigned long)time(NULL)^((unsigned long)getpid()<<16);
 char *x=strrchr(d,'X'); if(!x)return -1;
 for(unsigned attempt=0;attempt<100;++attempt){
  char *p=x; unsigned long v=seed+attempt*2654435761UL;
  for(;p<d+strlen(d);++p){*p=alphabet[v%36];v=v/36+seed;}
  if(!mkdir(d,0700))return 0;
  if(errno!=EEXIST)return -1;
 }
 return -1;
}
static int path_is_lab(const char *p){struct stat st;char m[CAP];return p&&p[0]=='/'&&!lstat(p,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==getuid()&&(st.st_mode&0777)==0700&&!pcat(m,p,"LAB_ONLY")&&!lstat(m,&st)&&S_ISREG(st.st_mode)&&st.st_uid==getuid()&&st.st_nlink==1;}
static int syncdir(const char *p){int fd=open(p,O_RDONLY),r;if(fd<0)return -1;r=fsync(fd);if(close(fd))r=-1;return r;}
static int make(const char *p,const char *s){int fd=open(p,O_WRONLY|O_CREAT|O_EXCL,0600),r=0;size_t n=strlen(s);if(fd<0)return -1;if(write(fd,s,n)!=(ssize_t)n||fchmod(fd,0640)||fsync(fd))r=-1;if(close(fd))r=-1;return r;}
static int matches(const char *p,const char *s){char b[64];struct stat st;int fd=open(p,O_RDONLY);ssize_t n;if(fd<0)return -1;n=read(fd,b,sizeof b);if(fstat(fd,&st)){close(fd);return -1;}if(close(fd))return -1;return n==(ssize_t)strlen(s)&&!memcmp(b,s,(size_t)n)&&(st.st_mode&0777)==0640&&st.st_uid==getuid()?0:-1;}
static int locking(const char *p){int fd=open(p,O_RDWR|O_CREAT|O_EXCL,0600),st,r=-1;struct flock lk;pid_t pid,q;if(fd<0)return -1;memset(&lk,0,sizeof lk);lk.l_type=F_WRLCK;lk.l_whence=SEEK_SET;if(fcntl(fd,F_SETLK,&lk))goto end;
pid=fork();if(pid<0)goto end;if(!pid){int other=open(p,O_RDWR),rc;if(other<0)_exit(3);rc=fcntl(other,F_SETLK,&lk);_exit(rc<0&&(errno==EACCES||errno==EAGAIN)?0:4);}
do{q=waitpid(pid,&st,0);}while(q<0&&errno==EINTR);if(q==pid&&WIFEXITED(st)&&!WEXITSTATUS(st))r=0;
end:close(fd);return r;}
int main(int argc,char **argv){char d[CAP],a[CAP],b[CAP],l[CAP];int rc=1;const char *stage="lab-root";
if(argc!=2||!path_is_lab(argv[1])||pcat(d,argv[1],"mu1005-probe-XXXXXX")||make_lab_dir(d)){fprintf(stderr,"FS_CAPABILITY_BLOCKED lab-root\n");return 2;}
if(pcat(a,d,"target")||pcat(b,d,"candidate")||pcat(l,d,"lock"))return 2;
#define CHECK(label,expr) do{stage=label;if((expr))goto done;printf("FS_CAPABILITY_OK %s\n",label);}while(0)
CHECK("parent-directory-fsync",syncdir(argv[1]));
CHECK("original-write-fsync",make(a,"original")||syncdir(d)||matches(a,"original"));
CHECK("candidate-write-fsync",make(b,"candidate")||syncdir(d)||matches(b,"candidate"));
CHECK("rename-replacement",rename(b,a)||syncdir(d)||matches(a,"candidate"));
CHECK("advisory-lock-exclusion",locking(l));
CHECK("restore-replacement",make(b,"original")||rename(b,a)||syncdir(d)||matches(a,"original"));
rc=0;
done:if(rc)fprintf(stderr,"FS_CAPABILITY_BLOCKED %s errno=%d\n",stage,errno);
/* Remove only names created inside our unique directory. */
unlink(a);unlink(b);unlink(l);if(rmdir(d)||syncdir(argv[1])){fprintf(stderr,"FS_CAPABILITY_BLOCKED cleanup\n");rc=1;}
if(!rc)puts("FS_CAPABILITY_FINISHED not_power_loss_proof");
return rc;
}
