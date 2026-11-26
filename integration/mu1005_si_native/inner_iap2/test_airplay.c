/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only ABI-shaped fake CFL library: checks adapter behavior,
 * not OEM ABI, runtime or authorization. */
#include "inner_airplay.h"
#include "inner_session.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
enum { DICT=1, STR=2, DATA=3, SESSION=4 };
typedef struct Obj {
 unsigned long type;
 const char *str;
 struct Obj *field[3];
 unsigned char data[64];
 long n;
} Obj;
static Obj pool[32],session={.type=SESSION};
static unsigned used;
static void *handle;
static int owner_live=1, sends, fail_send;
static uint64_t owner_epoch=7;
static Obj *last;
static MuInner relay;
static unsigned char marker[]={255,85,2,0,238,16};
unsigned char kCFLDictionaryKeyCallBacksCFLTypes[24];
unsigned char kCFLDictionaryValueCallBacksCFLTypes[20];
static Obj *make(unsigned long type){
 assert(used<sizeof pool/sizeof *pool);
 Obj *p=&pool[used++];memset(p,0,sizeof *p);p->type=type;return p;
}
unsigned long CFGetTypeID(const void *v){return ((const Obj*)v)->type;}
unsigned long CFDictionaryGetTypeID(void){return DICT;}
unsigned long CFStringGetTypeID(void){return STR;}
unsigned long CFDataGetTypeID(void){return DATA;}
unsigned long AirPlayReceiverSessionGetTypeID(void){return SESSION;}
void *CFDictionaryCreateMutable(const void *a,long n,const void *k,const void *v){
 assert(!a&&!n&&k==kCFLDictionaryKeyCallBacksCFLTypes&&v==kCFLDictionaryValueCallBacksCFLTypes);
 return make(DICT);
}
void *CFStringCreateWithCString(const void *a,const char *s,unsigned enc){
 assert(!a&&enc==0x08000100u);Obj *o=make(STR);o->str=s;return o;
}
static int slot(const void *v){
 const Obj *k=v;assert(k->type==STR);
 if(!strcmp(k->str,"type"))return 0;
 if(!strcmp(k->str,"params"))return 1;
 assert(!strcmp(k->str,"data"));return 2;
}
void CFDictionarySetValue(void *d,const void *k,const void *v){
 Obj *o=d;assert(o->type==DICT);o->field[slot(k)]=(Obj*)v;
}
const void *CFDictionaryGetValue(const void *d,const void *k){
 const Obj *o=d;assert(o->type==DICT);return o->field[slot(k)];
}
int CFDictionarySetCString(void *d,const void *k,const char *v,long n){
 assert(n==(long)strlen(v));Obj *o=make(STR);o->str=v;CFDictionarySetValue(d,k,o);return 0;
}
int CFDictionarySetData(void *d,const void *k,const unsigned char *v,long n){
 Obj *o=make(DATA);assert(n>0&&n<=(long)sizeof o->data);o->n=n;memcpy(o->data,v,(size_t)n);CFDictionarySetValue(d,k,o);return 0;
}
unsigned char CFEqual(const void *p,const void *q){
 const Obj *a=p,*b=q;assert(a->type==STR&&b->type==STR);return strcmp(a->str,b->str)==0;
}
long CFDataGetLength(const void *p){return ((const Obj*)p)->n;}
const unsigned char *CFDataGetBytePtr(const void *p){return ((const Obj*)p)->data;}
void CFRelease(const void *p){assert(p);}
static MuInnerAirplayCompletion pending_done;
static void *pending_context;
int AirPlayReceiverSessionSendCommand(void *s,const void *d,MuInnerAirplayCompletion done,void *ctx){
 assert(s==&session&&(done||!ctx)&&!pending_done);
 Obj *o=(Obj*)d;assert(o->type==DICT);
 assert(!strcmp(o->field[0]->str,"iAPSendMessage"));
 Obj *v=o->field[1];assert(v&&v->type==DICT&&v->field[2]->type==DATA);
 assert(v->field[2]->n==6&&!memcmp(v->field[2]->data,marker,6));
 last=o;sends++;if(fail_send)return -1;
 pending_done=done;pending_context=ctx;return 0;
}
static int completion_calls,completion_status;
static void *completion_expected;
static const void *completion_reply;
static void completion(int status,const void *reply,void *ctx){
 assert(ctx==completion_expected);
 completion_calls++;completion_status=status;completion_reply=reply;
}
static void deliver_completion(int status,const void *reply){
 MuInnerAirplayCompletion cb=pending_done;void *ctx=pending_context;
 assert(cb);pending_done=NULL;pending_context=NULL;cb(status,reply,ctx);
}
static int library(void *p,void *h){(void)p;return h==handle;}
static int owner(void *p,void *s,uint64_t e){(void)p;return owner_live&&s==&session&&e==owner_epoch;}
static int relay_owner(void *p,uint64_t e){(void)p;return owner_live&&e==7;}
static int write_cinemo(void *p,const unsigned char *b,size_t n){
 (void)p;assert(n==6&&!memcmp(b,marker,n));return (int)n;
}
static MuInnerSession joined;
static int stock_calls;
static const void *stock_expected;
static int stock_control(void *s,unsigned flags,const void *cmd,const void *qualifier,const void *params,void **response){
 assert(s==&session&&flags==1&&cmd==stock_expected&&!qualifier&&!params&&response&&!*response);
 stock_calls++;return -42;
}
static int peer_ok=1;
static int peer_proof(void *ctx,int fd,uint64_t e){return ctx==&joined&&fd>=0&&e==owner_epoch&&peer_ok;}
static void joined_test(const MuInnerAirplayProof *proof){
 int f[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,f)==0);
 for(int i=0;i<2;i++)assert(fcntl(f[i],F_SETFL,O_NONBLOCK)==0);
 owner_live=1;fail_send=0;used=0;sends=0;
 peer_ok=0;
 assert(mu_inner_session_start(&joined,handle,&session,7,proof,f[0],&joined,peer_proof)==-1);
 assert(fcntl(f[0],F_GETFD)>=0&&!joined.active&&!joined.airplay.bound);
 peer_ok=1;
 assert(mu_inner_session_start(&joined,handle,&session,7,proof,f[0],&joined,peer_proof)==1);
 assert(write(f[1],marker,2)==2);
 assert(mu_inner_session_tick(&joined,7)==1&&sends==0);
 assert(write(f[1],marker+2,4)==4);
 assert(mu_inner_session_tick(&joined,7)==1&&sends==0);
 assert(mu_inner_session_tick(&joined,7)==1&&sends==1);
 assert(mu_inner_session_control(&joined,6,&session,last->field[0],last->field[1])==-1&&joined.active);
 void *response=NULL;int status=123;
 Obj ordinary={.type=STR,.str="requestUI"};
 assert(mu_inner_session_http_command(&joined,7,&session,1,&ordinary,NULL,NULL,&response,&status)==0&&status==123);
 stock_expected=&ordinary;
 assert(mu_inner_session_dispatch_at(&joined,7,stock_control,0x10000000u,0x1002ccc8u,&session,1,&ordinary,NULL,NULL,&response)==-42&&stock_calls==1);
 assert(mu_inner_session_http_command(&joined,7,&session,0,last->field[0],NULL,last->field[1],&response,&status)==0&&status==123);
 /* Unrelated original UI control must pass through even if the
  * wireless relay lost its owner. Genuine iAPSendMessage still rejects. */
 owner_live=0;
 assert(mu_inner_session_dispatch_at(&joined,7,stock_control,0x10000000u,0x1002cadcu,&session,1,&ordinary,NULL,NULL,&response)==-42&&stock_calls==2);
 owner_live=1;
 assert(mu_inner_session_dispatch_at(&joined,7,stock_control,0x10000000u,0x1002cadcu,&session,1,last->field[0],NULL,last->field[1],&response)==0&&!response&&stock_calls==2);
 assert(mu_inner_session_http_command(&joined,7,&session,1,last->field[0],NULL,last->field[1],&response,&status)==1&&status!=0&&!response);
 assert(mu_inner_session_control(&joined,7,&session,last->field[0],last->field[1])==0);
 assert(mu_inner_session_tick(&joined,7)==1);
 unsigned char out[8];assert(read(f[1],out,sizeof out)==6&&!memcmp(out,marker,6));
 owner_live=0;
 assert(mu_inner_session_tick(&joined,7)==-1&&!joined.active&&!joined.airplay.bound);
 assert(fcntl(f[0],F_GETFD)==-1&&errno==EBADF);
 close(f[1]);mu_inner_session_stop(&joined);
 owner_live=1;used=0;
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,f)==0);
 for(int i=0;i<2;i++)assert(fcntl(f[i],F_SETFL,O_NONBLOCK)==0);
 assert(mu_inner_session_start(&joined,handle,&session,7,proof,f[0],&joined,peer_proof)==-1);
 assert(fcntl(f[0],F_GETFD)>=0);
 owner_epoch=8;
 assert(mu_inner_session_start(&joined,handle,&session,8,proof,f[0],&joined,peer_proof)==1);
 assert(mu_inner_session_tick(&joined,7)==-1&&joined.active);
 close(f[1]);assert(mu_inner_session_tick(&joined,8)==-1&&!joined.active);
 assert(fcntl(f[0],F_GETFD)==-1&&errno==EBADF);
 /* A real socket drives the optional native completion mailbox. */
 static MuInnerEventFlight flight;
 owner_epoch=9;used=0;sends=0;
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,f)==0);
 for(int i=0;i<2;i++)assert(fcntl(f[i],F_SETFL,O_NONBLOCK)==0);
 assert(mu_inner_session_start(&joined,handle,&session,9,proof,f[0],&joined,peer_proof)==1);
 assert(mu_inner_session_track_events(&joined,&flight)==1);
 assert(write(f[1],marker,6)==6&&mu_inner_session_tick_at(&joined,9,100)==1);
 assert(mu_inner_session_tick_at(&joined,9,101)==1&&pending_done&&sends==1);
 assert(write(f[1],marker,6)==6&&mu_inner_session_tick_at(&joined,9,102)==1);
 assert(mu_inner_session_tick_at(&joined,9,103)==1&&sends==1);
 deliver_completion(0,NULL);
 assert(mu_inner_session_tick_at(&joined,9,104)==1&&sends==2&&pending_done);
 assert(mu_inner_session_tick_at(&joined,9,5103)==1);
 assert(mu_inner_session_tick_at(&joined,9,5104)==-1&&!joined.active);
 assert(mu_inner_event_reclaim(&flight,0)==-1);
 deliver_completion(-1,NULL); /* late callback must not touch closed session */
 assert(mu_inner_event_reclaim(&flight,1)==1);
 close(f[1]);
 puts("MU1005_INNER_COMPOSED_SOCKET_AIRPLAY_PASS_HOST_FAKE_NO_OEM");
}
static union { uint32_t align; unsigned char bytes[0x160]; } job_manager;
static int job_path,job_posts,job_allowed=1,job_fail;
static uint64_t job_post(void *proxy,const void *path,unsigned exlap){
 assert(proxy==job_manager.bytes+0x130&&path==&job_path&&exlap==0);
 job_posts++;return job_fail?0:UINT64_C(0x123456789abc);
}
static int job_proof(void *ctx,void *manager,void *proxy,const void *path,unsigned exlap,uint64_t e,MuDioIapPost p){
 return job_allowed&&ctx==&job_path&&manager==job_manager.bytes&&proxy==job_manager.bytes+0x130&&
  path==&job_path&&!exlap&&e>=11&&p==job_post;
}
static void original_job_test(void){
 MuDioIapJob j={0};
 job_allowed=0;
 assert(mu_inner_dio_iap_post(&j,11,job_manager.bytes,&job_path,0,job_post,job_proof,&job_path)==-1&&!job_posts);
 job_allowed=1;
 assert(mu_inner_dio_iap_post(&j,11,job_manager.bytes,&job_path,1,job_post,job_proof,&job_path)==-1&&!job_posts);
 assert(mu_inner_dio_iap_post(&j,11,job_manager.bytes,&job_path,0,job_post,job_proof,&job_path)==1);
 assert(j.id==UINT64_C(0x123456789abc)&&job_posts==1);
 assert(mu_inner_dio_iap_post(&j,12,job_manager.bytes,&job_path,0,job_post,job_proof,&job_path)==-1&&job_posts==1);
 assert(mu_inner_dio_iap_retire(&j,11,0)==-1);
 assert(mu_inner_dio_iap_retire(&j,11,1)==1&&!j.id);
 assert(mu_inner_dio_iap_post(&j,11,job_manager.bytes,&job_path,0,job_post,job_proof,&job_path)==-1&&job_posts==1);
 job_fail=1;
 assert(mu_inner_dio_iap_post(&j,12,job_manager.bytes,&job_path,0,job_post,job_proof,&job_path)==-1&&j.attempted&&job_posts==2);
 assert(mu_inner_dio_iap_post(&j,13,job_manager.bytes,&job_path,0,job_post,job_proof,&job_path)==-1&&job_posts==2);
 puts("MU1005_ORIGINAL_DIO_JOB_ABI_HOST_FIXTURE_PASS_NO_OEM");
}
int main(void){
 original_job_test();
 handle=dlopen(NULL,RTLD_NOW);assert(handle);
 MuInnerAirplay a={0};MuInnerAirplayProof proof={0,library,owner};
 assert(mu_inner_ap_bind(&a,handle,&session,7,&proof)==1);
 MuInnerOps ops={&a,relay_owner,mu_inner_ap_send,write_cinemo};
 assert(mu_inner_start(&relay,7,&ops)==1);
 assert(mu_inner_from_cinemo(&relay,7,marker,6)==1);
 assert(mu_inner_pump(&relay,7)==1&&sends==1&&last);
 assert(mu_inner_ap_control(&a,&relay,7,last)==1);
 assert(mu_inner_ap_control(&a,&relay,7,last)==0);
 assert(mu_inner_pump(&relay,7)==1);
 assert(mu_inner_ap_control(&a,&relay,6,last)==-1);
 /* Reuse exact same decoded fields for the stock split-argument ABI. */
 Obj foreign_session={.type=SESSION};
 Obj *cmd=last->field[0],*arg=last->field[1];
 assert(mu_inner_ap_control_parts(&a,&relay,7,&foreign_session,cmd,arg)==-1);
 assert(mu_inner_ap_control_parts(&a,&relay,6,&session,cmd,arg)==-1);
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,NULL,arg)==-1);
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,arg,arg)==-1);
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,NULL)==-1);
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,arg)==1);
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,arg)==0);
 assert(mu_inner_pump(&relay,7)==1);
 Obj *saved_data=arg->field[2];
 arg->field[2]=cmd;
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,arg)==-1);
 arg->field[2]=saved_data;
 long saved_n=saved_data->n;
 saved_data->n=0;
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,arg)==-1);
 saved_data->n=(long)MU_INNER_CAP+1;
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,arg)==-1);
 saved_data->n=saved_n;
 owner_live=0;
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,cmd,arg)==-1);
 owner_live=1;
 Obj other={.type=STR,.str="otherCommand"};
 Obj *old=last->field[0];last->field[0]=&other;
 assert(mu_inner_ap_control(&a,&relay,7,last)==2);
 assert(mu_inner_ap_control_parts(&a,&relay,7,&session,&other,NULL)==2);
 last->field[0]=old;
 Obj *params=last->field[1];last->field[1]=NULL;
 assert(mu_inner_ap_control(&a,&relay,7,last)==-1);last->field[1]=params;
 fail_send=1;assert(mu_inner_from_cinemo(&relay,7,marker,6)==1);
 assert(mu_inner_pump(&relay,7)==-1&&!relay.active&&sends==2);
 owner_live=0;assert(mu_inner_ap_send(&a,marker,6)==-1&&sends==2);
 mu_inner_ap_unbind(&a);assert(!a.bound);
 joined_test(&proof);
 /* Original send returns queue admission, not completion. The completion
  * context belongs to the caller and must outlive unbind. */
 MuInnerAirplay tracked={0};int cookie=42;
 owner_live=1;owner_epoch=10;used=0;fail_send=0;
 assert(mu_inner_ap_bind(&tracked,handle,&session,10,&proof)==1);
 completion_expected=&cookie;
 assert(mu_inner_ap_send_with_completion(&tracked,marker,6,NULL,&cookie)==-1);
 assert(mu_inner_ap_send_with_completion(&tracked,marker,6,completion,&cookie)==1);
 assert(pending_done&&completion_calls==0);
 Obj reply={.type=DICT};
 deliver_completion(0,&reply);
 assert(completion_calls==1&&completion_status==0&&completion_reply==&reply);
 /* Keep the four bound CFL key objects alive until unbind. Resetting
  * the fake pool here overwrote key_type with a dictionary. */
 assert(mu_inner_ap_send_with_completion(&tracked,marker,6,completion,&cookie)==1);
 mu_inner_ap_unbind(&tracked);
 assert(pending_done&&completion_calls==1);
 deliver_completion(-1,NULL);
 assert(completion_calls==2&&completion_status==-1&&!completion_reply);
 puts("MU1005_NATIVE_EVENT_COMPLETION_ABI_HOST_PASS_NO_OEM");
 dlclose(handle);puts("MU1005_INNER_AIRPLAY_ENVELOPE_HOST_PASS_NO_OEM");return 0;
}
