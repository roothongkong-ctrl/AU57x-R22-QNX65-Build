/* SPDX-License-Identifier: GPL-3.0-only
 * Isolated loader/early-start observation. No OEM calls, authentication,
 * license changes, fake hardware, or network. A live PID is not CarPlay.
 */
#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 if(access("/dev/sdio0",F_OK)==0||access("/dev/i2c8",F_OK)==0)return 9;
 char path[]="/proc/boot/wcp_receiver";
 char *argv[]={path,NULL};
 char *env[]={"PATH=/proc/boot","LD_LIBRARY_PATH=/proc/boot",NULL};
 int fds[]={0,1,2};
 puts("DONOR_UNMODIFIED_ISOLATED_START");
 pid_t pid=spawn(path,3,fds,NULL,argv,env);
 if(pid<=0){printf("DONOR_SPAWN_FAILED errno=%d\n",errno);return 0;}
 for(int i=0;i<8;i++){
  int status=0;pid_t got=waitpid(pid,&status,WNOHANG);
  if(got==pid){
   if(WIFEXITED(status))printf("DONOR_EXIT status=%d\n",WEXITSTATUS(status));
   else if(WIFSIGNALED(status))printf("DONOR_SIGNAL signal=%d\n",WTERMSIG(status));
   else printf("DONOR_EXIT status_raw=%d\n",status);
   return 0;
  }
  if(got<0){printf("DONOR_WAIT_FAILED errno=%d\n",errno);return 2;}
  sleep(1);
 }
 puts("DONOR_ALIVE_AFTER_8S_NO_PHONE_NO_NETWORK");
 /* The entire disposable VM ends at its external timeout. */
 return 0;
}
