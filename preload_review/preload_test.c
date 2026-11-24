#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int child(int expected){
 void *h=dlopen(NULL,RTLD_NOW);if(!h)return 10;
 int (*pid)(void)=NULL;unsigned (*count)(void)=NULL;int (*readself)(void)=NULL;
 void *p=dlsym(h,"r137_preload_pid"),*c=dlsym(h,"r137_preload_count"),*r=dlsym(h,"r137_preload_readself");
 if(!expected){if(p||c||r)return 11;puts("R137_BASELINE_NO_PRELOAD_OK");return 0;}
 if(!p||!c||!r||sizeof p!=sizeof pid||sizeof c!=sizeof count||sizeof r!=sizeof readself)return 12;
 memcpy(&pid,&p,sizeof p);memcpy(&count,&c,sizeof c);memcpy(&readself,&r,sizeof r);
 if(pid()!=(int)getpid()||count()!=1)return 13;
 puts("R137_PRELOAD_CONSTRUCTOR_SAME_PID_OK");
#if defined(__QNXNTO__) && defined(__arm__)
 if(readself()!=1)return 14;
 puts("R137_QNX_READONLY_SELF_AS_OK");
#else
 if(readself()!=-2)return 15;
#endif
 return 0;
}
static int run(const char *exe,const char *lib,int expected){
 pid_t p=fork();if(p<0)return -1;
 if(p==0){if(expected){if(setenv("LD_PRELOAD",lib,1))_exit(20);}else if(unsetenv("LD_PRELOAD"))_exit(21);
  char *const args[]={(char*)exe,(char*)(expected?"preloaded":"baseline"),NULL};execv(exe,args);_exit(22);}
 int s=0;if(waitpid(p,&s,0)!=p||!WIFEXITED(s)||WEXITSTATUS(s))return -1;return 0;
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"baseline"))return child(0);
 if(argc==2&&!strcmp(argv[1],"preloaded"))return child(1);
#ifdef __QNXNTO__
 const char *exe="/proc/boot/r137_preload_test",*lib="/proc/boot/libau57x_r137_preload_REVIEW.so";
#else
 if(argc!=3)return 2;
 const char *exe=argv[1],*lib=argv[2];
#endif
 if(run(exe,lib,0)||run(exe,lib,1)||run(exe,lib,0))return 3;
 puts("AU57X_R137_PRELOAD_EXEC_ROLLBACK_OK_NO_OEM");return 0;
}
