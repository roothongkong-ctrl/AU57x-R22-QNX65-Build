/* Isolated original dependency diagnostics in QNX lab only. */
#if !defined(__QNXNTO__) || !defined(__arm__)
#error QNX ARM required
#endif
#include <dlfcn.h>
#include <process.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "profile.h"
static int test(unsigned i){
 if(i>=sizeof(deps)/sizeof(deps[0])-1)return 8;
 const char *n=deps[i];char path[256];
 void *p=dlopen(LAB_PRELOAD,RTLD_NOW|RTLD_GLOBAL);
 if(!p||!dlsym(p,"slogf")){puts("PIC_PROVIDER_CHILD_FAIL");return 9;}
 printf("DEP_ATTEMPT %s\n",n);
 if(snprintf(path,sizeof path,"/proc/boot/%s",n)>=(int)sizeof path)return 8;
 void *h=dlopen(path,RTLD_NOW|RTLD_LOCAL);
 if(!h){
  const char *e=dlerror();const char *reason="OTHER";
  if(e){
   if(strstr(e,"symbol")||strstr(e,"Symbol"))reason="SYMBOL";
   else if(strstr(e,"library")||strstr(e,"Library"))reason="LIBRARY";
   else if(strstr(e,"relocation")||strstr(e,"Relocation"))reason="RELOCATION";
  }
  printf("DEP_LOAD_FAIL %s %s\n",n,reason);return 2;
 }
 printf("DEP_LOAD_OK %s\n",n);
 /* No dlclose: process exit isolates original constructors/destructors. */
 return 0;
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);
 if(argc==2)return test((unsigned)strtoul(argv[1],NULL,10));
 unsigned count=sizeof(deps)/sizeof(deps[0])-1;
 printf("DEP_BEGIN %u\n",count);
 for(unsigned i=0;i<count;i++){
  char index[16];snprintf(index,sizeof index,"%u",i);
  char *args[]={argv[0],index,NULL};
  pid_t pid=spawnv(P_NOWAIT,"/proc/boot/mu1005-oem-launch",args);
  if(pid<0){printf("DEP_RESULT %s SPAWN_FAIL\n",deps[i]);continue;}
  int status=0,done=0;
  for(unsigned t=0;t<30;t++){
   pid_t n=waitpid(pid,&status,WNOHANG);
   if(n==pid){done=1;break;}
   if(n<0){done=-1;break;}
   usleep(100000);
  }
  if(done==1){
   if(WIFEXITED(status))printf("DEP_RESULT %s EXIT_%d\n",deps[i],WEXITSTATUS(status));
   else if(WIFSIGNALED(status))printf("DEP_RESULT %s SIGNAL_%d\n",deps[i],WTERMSIG(status));
   else printf("DEP_RESULT %s OTHER\n",deps[i]);
  }else if(!done){
   kill(pid,SIGKILL);waitpid(pid,&status,0);printf("DEP_RESULT %s TIMEOUT\n",deps[i]);
  }else printf("DEP_RESULT %s WAIT_FAIL\n",deps[i]);
 }
 puts("DEP_END_NOT_SERVICE_READY");return 0;
}
