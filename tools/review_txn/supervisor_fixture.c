#define _POSIX_C_SOURCE 200809L
#include "supervisor_fixture.h"
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef struct {const txn_spec *spec;const char *mode;pid_t pid;int stopfd,healthfd,stops;} lab;
static int mode(lab *l,const char *m){return !strcmp(l->mode,m);}
static void note(const char *s){puts(s);fflush(stdout);}
static void fdclose(int *fd){if(*fd>=0){close(*fd);*fd=-1;}}
static int stop(lab *l,int force){int st,i;pid_t q;struct timespec t={0,10000000};
 fdclose(&l->healthfd);fdclose(&l->stopfd);
 if(l->pid<=0)return 0;
 if(force)kill(l->pid,SIGKILL);
 for(i=0;i<100;i++){q=waitpid(l->pid,&st,WNOHANG);if(q==l->pid){l->pid=-1;return 0;}if(q<0&&errno!=EINTR)return -1;nanosleep(&t,NULL);}
 return -1;
}
static int quiesce(void *ctx){lab *l=ctx;l->stops++;note("quiesce");if((l->stops==1&&mode(l,"stop-original-fails"))||(l->stops==2&&mode(l,"stop-candidate-fails")))return -1;return stop(l,0);}
static int start_candidate(void *ctx){lab *l=ctx;int ctl[2],reply[2];pid_t p;note("start-candidate");
 if(mode(l,"start-fails"))return -1;
 if(pipe(ctl))return -1;
 if(pipe(reply)){close(ctl[0]);close(ctl[1]);return -1;}
 p=fork();if(p<0){close(ctl[0]);close(ctl[1]);close(reply[0]);close(reply[1]);return -1;}
 if(p==0){char c;ssize_t q;close(ctl[1]);close(reply[0]);
  if(!mode(l,"timeout")){c=mode(l,"bad-health")||mode(l,"stop-candidate-fails")?'N':'Y';if(write(reply[1],&c,1)!=1)_exit(90);close(reply[1]);}
  do{q=read(ctl[0],&c,1);}while(q<0&&errno==EINTR);_exit(q==0?0:91);
 }
 close(ctl[0]);close(reply[1]);l->pid=p;l->stopfd=ctl[1];l->healthfd=reply[0];
 if(mode(l,"crash-after-start"))_exit(79);
 return 0;
}
static int verify(void *ctx){lab *l=ctx;struct pollfd p={l->healthfd,POLLIN,0};int r,st;char c;note("verify-candidate");
 if(mode(l,"competing-lock")){
  pid_t child=fork();if(child<0)return -1;
  if(!child)_exit(txn_run(l->spec,"status")?0:1);
  do{r=waitpid(child,&st,0);}while(r<0&&errno==EINTR);
  if(r!=child||!WIFEXITED(st)||WEXITSTATUS(st)!=0)return -1;
  note("competitor-rejected");
 }
 r=poll(&p,1,200);if(r!=1||!(p.revents&POLLIN)||read(l->healthfd,&c,1)!=1||c!='Y')return -1;
 return 0;
}
static int start_original(void *ctx){lab *l=ctx;note("start-original");return mode(l,"restart-fails")?-1:0;}
int supervisor_fixture(const txn_spec *s,const char *action){lab l={s,getenv("TXN_LIFE_MODE"),-1,-1,-1,0};txn_lifecycle ops={&l,quiesce,start_candidate,verify,start_original};int r;if(!l.mode)l.mode="ok";
 r=!strcmp(action,"supervised-trial")?txn_supervised_trial(s,&ops):!strcmp(action,"supervised-restore")?txn_supervised_restore(s,&ops):!strcmp(action,"supervised-boot")?txn_supervised_boot(s,&ops):txn_supervised_recover(s,&ops);
 /* Fixture cleanup only. This does not perform target restoration. */
 if(stop(&l,0))stop(&l,1);
 return r==TXN_LIFE_COMMITTED?0:r==TXN_LIFE_ROLLED_BACK?3:1;
}
