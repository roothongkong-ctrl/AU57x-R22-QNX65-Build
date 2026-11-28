/* SPDX-License-Identifier: GPL-3.0-only
 * Isolated loader/early-start observation. No OEM calls, authentication,
 * license changes, fake hardware, or network. A live PID is not CarPlay.
 */
#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
static int stage_receiver(void){
 int src=open("/proc/boot/wcp_receiver",O_RDONLY);if(src<0)return -1;
 int dst=open("/dev/shmem/wcp_receiver",O_WRONLY|O_CREAT|O_EXCL,0755);
 if(dst<0){close(src);return -1;}
 char buf[8192];ssize_t n;int rc=0;
 while((n=read(src,buf,sizeof buf))>0){ssize_t off=0;while(off<n){ssize_t k=write(dst,buf+off,(size_t)(n-off));if(k<=0){rc=-1;break;}off+=k;}if(rc)break;}
 if(n<0||fchmod(dst,0755)!=0)rc=-1;
 if(close(dst)!=0)rc=-1;
 close(src);if(rc)unlink("/dev/shmem/wcp_receiver");return rc;
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 if(access("/dev/sdio0",F_OK)==0||access("/dev/i2c8",F_OK)==0)return 9;
 errno=0;
 char trial[]="/ramdisk/mu1005-runtime-probe";
 if(mkdir(trial,0700)!=0){printf("DONOR_RAMDISK_UNAVAILABLE errno=%d\n",errno);return 4;}
 if(rmdir(trial)!=0)return 4;
 puts("DONOR_RAMDISK_WRITABLE");
 if(stage_receiver()!=0){printf("DONOR_STAGE_FAILED errno=%d\n",errno);return 3;}
 if(access("/mnt/app/carplaymaps/libomonob-wireless-maps.so",R_OK)!=0||access("/mnt/app/carplaymaps/wireless-receiver-exec",X_OK)!=0){puts("DONOR_LAYOUT_UNAVAILABLE");return 3;}
 char path[]="/mnt/app/carplaymaps/wireless-receiver-exec";
 char *argv[]={path,NULL};
 char *env[]={"PATH=/proc/boot","LD_LIBRARY_PATH=/proc/boot","WCP_DATA=/mnt/persist/wcp","WCP_PORT=5000",NULL};
 int fds[]={0,1,2};
 puts("DONOR_ORIGINAL_LAUNCHER_ISOLATED_START");
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
 puts("DONOR_LAUNCHER_OR_CHILD_ALIVE_AFTER_8S_NO_PHONE");
 /* The entire disposable VM ends at its external timeout. */
 return 0;
}
