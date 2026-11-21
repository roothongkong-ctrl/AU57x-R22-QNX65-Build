#define _POSIX_C_SOURCE 200809L
/* Disposable emulator disk lab. Never names OEM files, mounts or services.
 * Caller must create a private LAB_ONLY root on a disposable writable disk. */
#include "txn.h"
#include "supervisor_fixture.h"
#include "sha256.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define N 4096
static const char *names[]={"one.lab","two.lab","three.lab"};
static const char *crash_phase;
static size_t crash_index;
static int join(char out[N],const char *d,const char *n){int i=snprintf(out,N,"%s/%s",d,n);return i<0||i>=N;}
static int lab_root(const char *p){struct stat st;char marker[N];return p&&p[0]=='/'&&!lstat(p,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==getuid()&&(st.st_mode&0777)==0700&&!join(marker,p,"LAB_ONLY")&&!lstat(marker,&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_uid==getuid();}
static void digest(const char *s,char h[65]){static const char hex[]="0123456789abcdef";sha256_ctx c;unsigned char b[32];size_t i;sha256_init(&c);sha256_update(&c,s,strlen(s));sha256_final(&c,b);for(i=0;i<32;i++){h[2*i]=hex[b[i]>>4];h[2*i+1]=hex[b[i]&15];}h[64]=0;}
static int create_file(const char *d,const char *name,const char *text){char p[N];int fd,rc;size_t n=strlen(text);if(join(p,d,name))return -1;fd=open(p,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return -1;rc=(write(fd,text,n)!=(ssize_t)n)||fchmod(fd,0640)||fsync(fd);if(close(fd))rc=1;return rc;}
static int original(const char *d,const char *name,const char *text){char p[N],buf[128];struct stat st;int fd;ssize_t n;if(join(p,d,name)||lstat(p,&st)||!S_ISREG(st.st_mode)||(st.st_mode&0777)!=0640||st.st_uid!=getuid()||st.st_gid!=getgid())return -1;fd=open(p,O_RDONLY);if(fd<0)return -1;n=read(fd,buf,sizeof buf);if(close(fd))return -1;return n==(ssize_t)strlen(text)&&!memcmp(buf,text,(size_t)n)?0:-1;}
static void checkpoint(const char *phase,size_t i){if(!strcmp(phase,crash_phase)&&i==crash_index)_exit(77);}
static int sync_dir(const char *p){int fd=open(p,O_RDONLY),rc;if(fd<0)return -1;rc=fsync(fd);if(close(fd))rc=-1;return rc;}
/* QNX 6.5 headers lack mkdtemp. mkdir is exclusive; never reuse a directory. */
static int new_dir(char out[N],const char *root){unsigned i;int n;for(i=0;i<1024;i++){n=snprintf(out,N,"%s/mu1005-recovery-%lu-%u",root,(unsigned long)getpid(),i);if(n<0||n>=N)return -1;if(!mkdir(out,0700))return 0;if(errno!=EEXIST)return -1;}return -1;}
static int scenario(const char *root,const char *phase,size_t index){
 char dir[N],dirs[4][N],removed[N],old[3][65],newh[3][65],text[3][64],candidate[64];const char *sub[]={"target","backup","payload","state"};txn_row rows[3];txn_spec spec;size_t i;pid_t child,w;int status;
 if(new_dir(dir,root))return -1;
 printf("LAB_DIRECTORY=%s\n",dir);fflush(stdout);
 for(i=0;i<4;i++)if(join(dirs[i],dir,sub[i])||mkdir(dirs[i],0700))return -1;
 for(i=0;i<3;i++){
  snprintf(text[i],sizeof text[i],"original %s",names[i]);snprintf(candidate,sizeof candidate,"candidate %s",names[i]);digest(text[i],old[i]);digest(candidate,newh[i]);
  rows[i]=(txn_row){names[i],old[i],newh[i],0640,0640,getuid(),getgid()};
  if(create_file(dirs[0],names[i],text[i])||create_file(dirs[2],names[i],candidate))return -1;
 }
 if(sync_dir(dirs[0])||sync_dir(dirs[2])||sync_dir(dir)||sync_dir(root))return -1;
 spec=(txn_spec){dirs[0],dirs[1],dirs[2],dirs[3],rows,3,checkpoint};crash_phase=phase;crash_index=index;
 if(!strncmp(phase,"boot-",5)){
  int want_original=strcmp(phase,"boot-ok")!=0;
  spec.checkpoint=NULL;
  /* Fixture-only confirmation; no production health claim. */
  if(txn_run(&spec,"trial")||txn_run(&spec,"confirm"))return -1;
  if(join(removed,dir,"removed-card")||rename(dirs[2],removed)||sync_dir(dir))return -1;
  if(!strcmp(phase,"boot-crash")){
   if(setenv("TXN_LIFE_MODE","crash-after-start",1))return -1;
   child=fork();if(child<0)return -1;if(!child){(void)supervisor_fixture(&spec,"supervised-boot");_exit(99);}
   do{w=waitpid(child,&status,0);}while(w<0&&errno==EINTR);
   if(w!=child||!WIFEXITED(status)||WEXITSTATUS(status)!=79)return -1;
   if(setenv("TXN_LIFE_MODE","ok",1))return -1;
  }else if(setenv("TXN_LIFE_MODE",want_original?"bad-health":"ok",1))return -1;
  if(supervisor_fixture(&spec,"supervised-boot")!=(want_original?3:0)||txn_run(&spec,"status"))return -1;
  for(i=0;i<3;i++){
   if(want_original){if(original(dirs[0],names[i],text[i]))return -1;}
   else{snprintf(candidate,sizeof candidate,"candidate %s",names[i]);if(original(dirs[0],names[i],candidate))return -1;}
  }
  printf("LAB_BOOT_OK %s missing_payload_verified\n",phase);fflush(stdout);return 0;
 }
 child=fork();if(child<0)return -1;if(!child){(void)txn_run(&spec,"trial");_exit(99);}
 do{w=waitpid(child,&status,0);}while(w<0&&errno==EINTR);
 if(w!=child||!WIFEXITED(status)||WEXITSTATUS(status)!=77)return -1;
 printf("LAB_CRASH_OK %s %lu\n",phase,(unsigned long)index);fflush(stdout);
 if(join(removed,dir,"removed-card")||rename(dirs[2],removed)||sync_dir(dir))return -1;
 spec.checkpoint=NULL;
 if(txn_run(&spec,"recover")||txn_run(&spec,"recover")||txn_run(&spec,"status"))return -1;
 for(i=0;i<3;i++)if(original(dirs[0],names[i],text[i]))return -1;
 puts("LAB_RECOVERY_OK missing_payload_original_verified");fflush(stdout);
 /* Preserve the private scenario files for post-run inspection. */
 return 0;
}
int main(int argc,char **argv){if(argc!=2||!lab_root(argv[1])){fputs("LAB_BLOCKED invalid private lab root\n",stderr);return 2;}
 if(scenario(argv[1],"apply",1)||scenario(argv[1],"pending",0)||scenario(argv[1],"boot-ok",0)||scenario(argv[1],"boot-health",0)||scenario(argv[1],"boot-crash",0)){fputs("LAB_BLOCKED inspect evidence directory\n",stderr);return 1;}
 puts("LAB_FINISHED process_interruptions_only not_power_loss_or_vehicle_proof");return 0;}
