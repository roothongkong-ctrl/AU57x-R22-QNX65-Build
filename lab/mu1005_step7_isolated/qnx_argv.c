/* Step7 harmless ARM/QNX argument target and shell-exec supervisor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <process.h>
#include <errno.h>
#include <sys/stat.h>
extern char **environ;
#ifdef TARGET
int main(int argc,char **argv) {
 const char *which=getenv("STEP7_CASE");
 if(!which || getenv("AU57X_R92_PROBE") || getenv("LD_PRELOAD")) return 41;
 if(!strcmp(which,"factory")) { if(argc!=2 || strcmp(argv[1],"1")) return 42; }
 else if(!strcmp(which,"empty")) { if(argc!=4 || strcmp(argv[1],"") || strcmp(argv[2],"a b") || strcmp(argv[3],"$;*")) return 43; }
 else if(!strcmp(which,"none")) { if(argc!=1) return 44; }
 else if(!strcmp(which,"missing")) { if(argc!=2 || strcmp(argv[1],"1")) return 45; }
 else if(!strcmp(which,"exit")) { if(argc!=2 || strcmp(argv[1],"1")) return 46; }
 else return 47;
 printf("STEP7_TARGET %s pid=%ld argc=%d\n",which,(long)getpid(),argc);fflush(stdout);
 return !strcmp(which,"exit")?23:0;
}
#else
static int check(const char *name,const char *flag,char *const *av,int expected) {
 int status=0;pid_t pid;
 if(setenv("STEP7_CASE",name,1)||setenv("AU57X_R92_PROBE_OPT_IN",flag,1))return 1;
 unsetenv("AU57X_R92_PROBE");unsetenv("LD_PRELOAD");
 struct stat st;
 if(stat("/bin/sh",&st)){perror("STEP7 /bin/sh stat");return 2;}
 if(stat("/proc/boot/step7-wrap",&st)){perror("STEP7 wrapper stat");return 2;}
 if(stat("/proc/boot/step7-target",&st)){perror("STEP7 target stat");return 2;}
 pid=fork();
 if(pid<0){perror("STEP7 fork");return 2;}
 if(pid==0){execve("/bin/sh",av,environ);fprintf(stderr,"STEP7_EXECVE_FAIL %s errno=%d\n",name,errno);_exit(125);}

 if(waitpid(pid,&status,0)!=pid || !WIFEXITED(status) || WEXITSTATUS(status)!=expected){printf("STEP7_QNX_FAIL %s status=%d\n",name,status);return 3;}
 printf("STEP7_QNX_ARGV_PASS %s pid=%ld status=%d\n",name,(long)pid,expected);fflush(stdout);
 return 0;
}
int main(void){
 char *one[]={"sh","/proc/boot/step7-wrap","1",NULL};
 char *none[]={"sh","/proc/boot/step7-wrap",NULL};
 int failed;
 #if STEP7_CASE_ID == 1
 (void)one;
 failed=check("none","0",none,0);
 #elif STEP7_CASE_ID == 2
 (void)none;
 failed=check("missing","1",one,0);
 #elif STEP7_CASE_ID == 3
 (void)none;
 failed=check("exit","0",one,23);
 #else
 #error Invalid STEP7_CASE_ID
 #endif
 if(failed)return 1;
 puts("R105164_QNX_ISOLATED_ARGV_PASS_NO_OEM");return 0;
}
#endif
