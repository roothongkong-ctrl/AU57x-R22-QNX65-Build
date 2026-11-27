/* QNX guest only; no car or hardware writes. */
#include <sys/wait.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "profile.h"
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 const char *path="/proc/boot/connectivity_launcher";
 int f=open(path,O_RDONLY);if(f<0)return 2;
 uint32_t h=2166136261u;unsigned long total=0;unsigned char b[4096];ssize_t n;
 while((n=read(f,b,sizeof b))>0){total+=(unsigned long)n;for(ssize_t i=0;i<n;i++)h=(h^b[i])*16777619u;}
 close(f);if(n<0||total!=PROGRAM_SIZE||h!=PROGRAM_FNV)return 3;
 puts("STEP8_STOCK_LAUNCHER_BYTES_OK");
 if(setenv("LD_PRELOAD","/proc/boot/liblab-slogf.so:/proc/boot/libstep8observe.so",1))return 4;
 char *args[]={"connectivity_launcher",NULL};
 pid_t p=spawnv(P_NOWAIT,path,args);unsetenv("LD_PRELOAD");
 if(p<0){perror("STEP8_STOCK_SPAWN");return 5;}
 printf("STEP8_STOCK_PID %d\n",(int)p);
 int s=0;for(int i=0;i<15;i++){pid_t r=waitpid(p,&s,WNOHANG);if(r==p){printf("STEP8_STOCK_EXIT %d\n",s);return 0;}if(r<0)return 6;sleep(1);}
 kill(p,SIGTERM);sleep(1);if(waitpid(p,&s,WNOHANG)==0){kill(p,SIGKILL);waitpid(p,&s,0);}
 puts("STEP8_STOCK_OBSERVATION_FINISHED");return 0;
}
