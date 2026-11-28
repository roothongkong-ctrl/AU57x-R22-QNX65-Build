/* Host-only observed command contract; no OEM executable is run. */
#include "dio_launch.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int calls;
static int admit(void*v,uint64_t e,const char*lib){
 (void)v;assert(e==9);assert(!strcmp(lib,"/tmp/libmu1005_review.so"));++calls;return 1;
}
int main(void){
 const char *lib="/tmp/libmu1005_review.so";
 char *env[]={"PATH=/bin",NULL};
 char *bad_env[]={"LD_PRELOAD=/tmp/other.so",NULL};
 char *bad_wireless[]={"WCP_DIO_WIRELESS=0",NULL};
 char *bad_review[]={"MU1005_DIO_INTERFACE_REVIEW=0",NULL};
 char *alias[]={"/tmp/dio_manager",NULL};
 char *extra[]={"/mnt/app/eso/bin/apps/dio_manager","--wireless",NULL};
 char *exact[]={"/mnt/app/eso/bin/apps/dio_manager",NULL};
 MuDioLaunch s={0};
 assert(mu_dio_launch_wireless(&s,9,alias,env,lib,admit,NULL)==-1);
 assert(mu_dio_launch_wireless(&s,9,extra,env,lib,admit,NULL)==-1);
 assert(mu_dio_launch_wireless(&s,9,NULL,bad_env,lib,admit,NULL)==-1);
 assert(mu_dio_launch_wireless(&s,9,NULL,bad_wireless,lib,admit,NULL)==-1);
 assert(mu_dio_launch_wireless(&s,9,NULL,bad_review,lib,admit,NULL)==-1);
 assert(!calls&&!s.attempted);
 assert(mu_dio_launch_wireless(&s,9,exact,env,lib,admit,NULL)==-1);
 assert(calls==1&&s.attempted&&s.pid==-1&&s.epoch==9);
 memset(&s,0,sizeof s);
 assert(mu_dio_launch_wireless(&s,9,NULL,env,lib,admit,NULL)==-1);
 assert(calls==2&&s.attempted&&s.pid==-1&&s.epoch==9);
 assert(!strcmp(env[0],"PATH=/bin")&&!env[1]);
 puts("R105301_R106_OBSERVED_DIO_ARGV_CONTRACT_PASS_HOST_ONLY");
 return 0;
}
