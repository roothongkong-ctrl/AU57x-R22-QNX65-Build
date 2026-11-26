/* Linux-host test only: real R85 C client/server, synthetic OEM callbacks. */
#define _GNU_SOURCE
#include "si_mu1005_cprx.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define B 0x10000u
#define P (B+MU_SI_DP_OFF)
#define ARR (B+0x2000u)
#define REC (B+0x2100u)
static unsigned char mem[0x3000];
static int adds,activates,cleaned;
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static void pause_tick(void){struct timespec t={0,1000000};nanosleep(&t,0);}
static void put(uint32_t a,uint32_t v){assert(a>=B&&a-B+4<=sizeof mem);for(unsigned i=0;i<4;i++)mem[a-B+i]=(unsigned char)(v>>(8*i));}
static int rd(void*c,uint32_t a,void*out,size_t n){(void)c;if(a<B||a-B>sizeof mem||n>sizeof mem-(a-B))return 0;memcpy(out,mem+a-B,n);return 1;}
static void idle(void){put(P+MU_NATIVE_APP_OFF,UINT32_MAX);put(P+MU_NATIVE_STATE_OFF,0);put(P+MU_NATIVE_ID_OFF,0);put(B+MU_SI_DEFERRED_APP_OFF,UINT32_MAX);put(B+MU_SI_DEFERRED_ID_OFF,UINT32_MAX);}
static void active(void){idle();put(P+MU_NATIVE_APP_OFF,0);put(P+MU_NATIVE_STATE_OFF,5);put(P+MU_NATIVE_ID_OFF,17);}
static int own(void*c,uint32_t p,uint64_t e,uint32_t s){(void)c;return p==P&&e==5&&s==8;}
static int admit(void*c,const struct mu_owner_session*k){(void)c;return k->controller==B&&k->epoch==5&&k->session==8;}
static int32_t add(void*p,void*k,void*secondary){assert((uintptr_t)p==P&&k&&secondary);adds++;put(P+MU_RECORD_ARRAY_COUNT_OFF,1);put(P+MU_RECORD_ARRAY_PTR_OFF,ARR);put(ARR,REC);put(REC,17);return 17;}
static void update(void*p,uint32_t id,void*info){assert((uintptr_t)p==P&&id==17&&info);}
static void activate(void*p,uint32_t id,uint32_t app){assert((uintptr_t)p==B&&id==17&&app==0);activates++;put(B+MU_SI_DEFERRED_APP_OFF,0);put(B+MU_SI_DEFERRED_ID_OFF,id);}
static int cleanup(void*c,const struct mu_owner_session*k,uint32_t id){return admit(c,k)&&id==17&&cleaned;}
struct fixture{pid_t peer;int starts,samples,restores,first_video;};
static int tuple(void*c,uint32_t g,uint64_t e,uint32_t s){return c&&g==42&&e==5&&s==8;}
static int peer(void*p,int fd,uint32_t g,uint64_t e,uint32_t s){struct fixture*f=p;struct ucred u;socklen_t n=sizeof u;return tuple(p,g,e,s)&&getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&u,&n)==0&&n==sizeof u&&u.pid==f->peer&&u.uid==geteuid()&&u.gid==getegid();}
static int admission(void*p,uint32_t g){return p&&g==42;}
static int identity(void*p,char*out,size_t cap){if(!p||cap<7)return -1;memcpy(out,"id-123",6);return 6;}
static int media(void*p){struct fixture*f=p;return ++f->samples<=2?7:0;}
static int auth(void*p,uint32_t g,uint64_t e,uint32_t s){return tuple(p,g,e,s)?((struct fixture*)p)->samples==1:-1;}
static int channel(void*p,uint32_t g,uint64_t e,uint32_t s,unsigned ch){if(!tuple(p,g,e,s)||ch<1||ch>4)return -1;return ((struct fixture*)p)->first_video&&ch!=1?-1:1;}
static int start(void*p,unsigned port){struct fixture*f=p;return port==5000&&++f->starts==1?2:-1;}
static int poll_backend(void*p){return p?2:-1;}
static int restore(void*p){struct fixture*f=p;return ++f->restores==1?1:0;}
static int server(const char*dir,pid_t parent,int first_video){
 struct fixture f={.peer=parent,.first_video=first_video};
 MuReceiverBackend b={&f,42,start,poll_backend,restore};
 MuCprxEvidence e={.opaque=&f,.admitted=admission,.identity=identity,.media=media,.si_admitted=tuple,.si_client_verified=peer,.si_inner_iap2_mfi=auth,.si_media_channel=channel};
 MuCprxService s={0};
 if(mu_cprx_open_backend_si_unix(&s,&b,&e,5000,5,8,dir,geteuid(),getegid()))return 2;
 uint64_t until=now()+14000;
 while(now()<until){int rc=mu_cprx_tick(&s,now());if(rc==1)return f.starts==1&&f.samples==3&&f.restores==2?0:3;if(rc<0){mu_cprx_shutdown(&s);return 4;}pause_tick();}
 mu_cprx_shutdown(&s);return 5;
}
static int transact(MuCprxRemoteEndpoint*e,unsigned op){
 MuCprxClient c;mu_cprx_client_init(&c);
 if(mu_cprx_client_begin_remote(&c,e,op,now())<0)return -1;
 uint64_t until=now()+7000;int rc=1;
 while(now()<until&&(rc=mu_cprx_client_tick(&c,now()))==1)pause_tick();
 mu_cprx_client_close(&c);return rc;
}
static int drive(struct mu_si_cprx*b){uint64_t until=now()+7500;int rc=1;while(now()<until&&(rc=mu_si_cprx_tick(b,now(),NULL,NULL,0))==1)pause_tick();return rc;}
int main(int argc,char**argv){
 int first_video=argc==2&&!strcmp(argv[1],"first-video");
 char dir[]="/tmp/au57x-review-XXXXXX";assert(mkdtemp(dir));
 pid_t child=fork();assert(child>=0);if(child==0)_exit(server(dir,getppid(),first_video));
 struct fixture f={.peer=child};
 MuCprxRemoteEndpoint e={.generation=42,.session=8,.epoch=5,.airplay_port=5000,.server_uid=geteuid(),.server_gid=getegid(),.owner_ctx=&f,.si_owner_verified=tuple,.dio_server_verified=peer};
 assert(snprintf(e.socket_path,sizeof e.socket_path,"%s/cprx.sock",dir)>0);
 struct stat st;uint64_t until=now()+3000;while(lstat(e.socket_path,&st)&&now()<until)pause_tick();
 assert(!lstat(e.socket_path,&st));
 assert(transact(&e,1)==MU_CPRX_CLIENT_CONFIRMED);
 puts("PASS real R85 START across processes");
 struct mu_owner_ops o={.registration={.owner_session_current=own,.add_unique_id=add,.update_record=update,.reader={0,rd}},.admitted=admit,.activate=activate,.cleanup_confirmed=cleanup};
 struct mu_owner_session k={B,8,5};struct mu_si_cprx b={0};uint64_t key[5]={0},info[20]={0};
 idle();assert((first_video?mu_si_cprx_init_first_video(&b,&o,&k,&e):mu_si_cprx_init(&b,&o,&k,&e))==0);
 assert(mu_si_cprx_tick(&b,now(),key,info,sizeof info)==MU_OWNER_PENDING);
 active();assert(drive(&b)==MU_OWNER_CONNECTED&&adds==1&&activates==1&&!b.proof);
 puts(first_video?"PASS real op11 first video with optional channels unknown":"PASS real op11 with exact session and all channels");
 assert(drive(&b)==MU_OWNER_UNCERTAIN);
 puts("PASS real op11 loss of authentication latches uncertainty");
 assert(mu_si_cprx_cleanup(&b)==MU_OWNER_UNCERTAIN);
 assert(transact(&e,2)==MU_CPRX_CLIENT_CONFIRMED);
 int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 idle();cleaned=1;assert(mu_si_cprx_cleanup(&b)==0&&!b.initialized);
 assert(!rmdir(dir));
 puts("PASS real R85 STOP and separately witnessed SI cleanup");
 puts("HOST_REAL_CPRX_OEM_FIXTURES_ONLY_NO_INSTALL");return 0;
}
