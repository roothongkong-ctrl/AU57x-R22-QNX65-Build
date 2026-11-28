/* SPDX-License-Identifier: GPL-3.0-only */
/* Real dlsym and coordinator; mocked OEM bind/open only. */
#include "dio_existing_receiver.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
static int server,manager,live=1,admit=1,binds,opens,restores,open_fail,restore_fail;
void *gAirPlayReceiverServer=&server;
static int verify(void*p,void*h){return p==&manager&&h&&live;}
static int owner(void*p,void*s,const void*m){return p==m&&m==&manager&&s==&server&&live;}
static int admitted(void*p,uint32_t g){return p==&manager&&g==7&&admit;}
static int si(void*p,uint32_t g,uint64_t e,uint32_t s){return admitted(p,g)&&e==8&&s==9;}
static int peer(void*p,int fd,uint32_t g,uint64_t e,uint32_t s){return fd>=0&&si(p,g,e,s);}
int mu_dio_bind_interface(MuDioNative*n,void*h,void*s,const void*m,uint32_t gen,
 const MuDioGuard*g,const char*orig,const char*target){
 assert(h&&owner(g->opaque,s,m)&&gen==7&&!strcmp(orig,"carplay0")&&!strcmp(target,"uap0"));
 binds++;n->initialized=1;return 0;
}
int mu_cprx_open_si_unix(MuCprxService*s,MuDioNative*n,const MuCprxEvidence*e,
 unsigned port,uint64_t epoch,uint32_t session,const char*dir,uid_t uid,gid_t gid){
 assert(s&&n->initialized&&e&&port==5000&&epoch==8&&session==9&&dir&&uid==0&&gid==0);
 opens++;return open_fail?-1:0;
}
int mu_dio_restore(MuDioNative*n){restores++;if(restore_fail)return 1;n->initialized=0;return 0;}
int main(void){
 void *h=dlopen(NULL,RTLD_NOW);assert(h);
 MuDioNative n={0};MuCprxService s={0};
 MuDioGuard g={.opaque=&manager,.verified_library=verify,.owner_exclusive=owner};
 MuCprxEvidence e={.opaque=&manager,.admitted=admitted,.si_admitted=si,.si_client_verified=peer};
 #define OPEN() mu_dio_existing_receiver_open(&s,&n,h,&manager,7,&g,"carplay0","uap0",&e,5000,8,9,"/fixture",0,0)
 live=0;assert(OPEN()==-1&&!binds);live=1;
 admit=0;assert(OPEN()==-1&&!binds);admit=1;
 gAirPlayReceiverServer=NULL;assert(OPEN()==-1&&!binds);gAirPlayReceiverServer=&server;
 open_fail=1;assert(OPEN()==-1&&binds==1&&opens==1&&restores==1&&!n.initialized);
 restore_fail=1;assert(OPEN()==-2&&n.initialized&&restores==2);
 assert(OPEN()==-1&&binds==2);restore_fail=0;assert(mu_dio_restore(&n)==0);
 open_fail=0;assert(OPEN()==0&&binds==3&&opens==3&&n.initialized);
 assert(OPEN()==-1&&binds==3);assert(mu_dio_restore(&n)==0);
 dlclose(h);puts("EXISTING_DIO_SINGLETON_COORDINATOR_PASS_FIXTURE_ONLY");return 0;
}
