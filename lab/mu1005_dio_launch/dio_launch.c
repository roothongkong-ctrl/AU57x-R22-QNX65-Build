/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "dio_launch.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#define STOCK "/mnt/app/eso/bin/apps/dio_manager"
static int reserved(const char*s){
 return !strncmp(s,"LD_PRELOAD=",sizeof("LD_PRELOAD=")-1)||
 !strncmp(s,"WCP_DIO_WIRELESS=",sizeof("WCP_DIO_WIRELESS=")-1)||
 !strncmp(s,"MU1005_DIO_INTERFACE_REVIEW=",sizeof("MU1005_DIO_INTERFACE_REVIEW=")-1);
}
static int path_ok(const char*p){
 if(!p||p[0]!='/'||strlen(p)>240)return 0;
 for(;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||
 *p=='/'||*p=='_'||*p=='-'||*p=='.'))return 0;
 return 1;
}
int mu_dio_launch_wireless(MuDioLaunch*s,uint64_t epoch,char*const argv[],char*const env[],
 const char*lib,int(*admit)(void*,uint64_t,const char*),void*ctx){
 char preload[280];char *child[128];unsigned n=0;
 char *observed_argv[]={STOCK,NULL};
 if(!s||s->attempted||s->pid>0||!epoch||!env||!path_ok(lib)||!admit)return -1;
 /* R106 actual wired process argv contains exactly this absolute path.
  * NULL explicitly opts into that observed form. It is NOT evidence of
  * launch authority or environment. Never invent a wireless command flag. */
 if(!argv)argv=observed_argv;
 if(!argv[0]||strcmp(argv[0],STOCK)||argv[1])return -1;
 /* The stock launch environment must be unambiguous. Ordinary USB is
  * never passed to this function; preexisting preload is not overwritten. */
 for(;env[n];++n){if(n>=124||reserved(env[n]))return -1;child[n]=env[n];}
 int k=snprintf(preload,sizeof preload,"LD_PRELOAD=%s",lib);
 if(k<0||(unsigned)k>=sizeof preload)return -1;
 child[n++]=preload;child[n++]="WCP_DIO_WIRELESS=1";child[n++]="MU1005_DIO_INTERFACE_REVIEW=1";child[n]=0;
 if(admit(ctx,epoch,lib)!=1)return -1;
 s->attempted=1;s->epoch=epoch;
#if defined(__QNXNTO__)
 int map[]={0,1,2};
 s->pid=spawn(STOCK,3,map,NULL,argv,child);
#else
 /* The host validates the constructed environment without launching OEM. */
 (void)child;
 s->pid=-1;
#endif
 if(s->pid<=0)return -1;
 /* A PID is not receiver readiness. Retain PID even if SI revokes admission. */
 return admit(ctx,epoch,lib)==1?0:-2;
}
int mu_dio_launch_reap(MuDioLaunch*s){
 if(!s||s->pid<=0)return -1;
 int status;pid_t r=waitpid(s->pid,&status,WNOHANG);
 if(r==0)return 1;
 if(r==s->pid){s->pid=0;return 0;}
 return -1;
}
