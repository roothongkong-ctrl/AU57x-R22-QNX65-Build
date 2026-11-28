/* Isolated diagnostic of real QNX prerequisites; never runs or patches donor. */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "parent.h"
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 if(access("/dev/sdio0",F_OK)==0||access("/dev/i2c8",F_OK)==0)return 9;
 struct timespec ts;
 errno=0;int rc=clock_gettime((clockid_t)2,&ts),err=errno;
 printf("DONOR_CLOCK2 rc=%d errno=%d CLOCK_MONOTONIC=%d\n",rc,err,(int)CLOCK_MONOTONIC);
 char path[180];int n=snprintf(path,sizeof path,"%s/mu1005-dir-probe-%ld",DONOR_PARENT,(long)getpid());
 if(n<=0||(unsigned)n>=sizeof path)return 2;
 errno=0;rc=mkdir(path,0700);err=errno;
 printf("DONOR_MKDIR parent=%s rc=%d errno=%d\n",DONOR_PARENT,rc,err);
 if(rc==0){if(rmdir(path)!=0)return 3;}
 return 0;
}
