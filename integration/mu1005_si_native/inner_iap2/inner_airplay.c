/* SPDX-License-Identifier: GPL-3.0-only */
#include "inner_airplay.h"
#include <dlfcn.h>
#include <string.h>
#define UTF8 0x08000100u
static void *lookup(void *h,const char *n){
 (void)dlerror();void *p=dlsym(h,n);return dlerror()?NULL:p;
}
static int sym(void *h,const char *n,void *out,size_t z){
 void *p=lookup(h,n);if(!p||z!=sizeof p)return -1;memcpy(out,&p,z);return 0;
}
static int live(MuInnerAirplay *a){
 return a&&a->bound&&a->proof.library(a->proof.ctx,a->handle)==1&&
  a->proof.owner(a->proof.ctx,a->session,a->epoch)==1;
}
void mu_inner_ap_unbind(MuInnerAirplay *a){
 if(!a)return;
 /* Owned immutable strings only; the caller keeps the library mapped. */
 if(a->release){
  if(a->key_type)a->release(a->key_type);
  if(a->key_params)a->release(a->key_params);
  if(a->key_data)a->release(a->key_data);
  if(a->cmd)a->release(a->cmd);
 }
 memset(a,0,sizeof *a);
}
int mu_inner_ap_bind(MuInnerAirplay *a,void *h,void *s,uint64_t e,const MuInnerAirplayProof *p){
 if(!a||a->bound||!h||!s||!e||!p||!p->library||!p->owner||
    p->library(p->ctx,h)!=1||p->owner(p->ctx,s,e)!=1)return -1;
 memset(a,0,sizeof *a);a->proof=*p;a->handle=h;a->session=s;a->epoch=e;
#define S(field,name) if(sym(h,name,&a->field,sizeof a->field))goto bad
 S(type,"CFGetTypeID");S(dict_type,"CFDictionaryGetTypeID");S(data_type,"CFDataGetTypeID");
 S(string_type,"CFStringGetTypeID");S(session_type,"AirPlayReceiverSessionGetTypeID");
 S(dict,"CFDictionaryCreateMutable");S(string,"CFStringCreateWithCString");
 S(set_string,"CFDictionarySetCString");S(set_data,"CFDictionarySetData");
 S(set,"CFDictionarySetValue");S(get,"CFDictionaryGetValue");S(equal,"CFEqual");
 S(length,"CFDataGetLength");S(bytes,"CFDataGetBytePtr");S(release,"CFRelease");
 S(send,"AirPlayReceiverSessionSendCommand");
#undef S
 a->key_callbacks=lookup(h,"kCFLDictionaryKeyCallBacksCFLTypes");
 a->value_callbacks=lookup(h,"kCFLDictionaryValueCallBacksCFLTypes");
 if(!a->key_callbacks||!a->value_callbacks||a->type(s)!=a->session_type())goto bad;
 a->key_type=a->string(NULL,"type",UTF8);
 a->key_params=a->string(NULL,"params",UTF8);
 a->key_data=a->string(NULL,"data",UTF8);
 a->cmd=a->string(NULL,"iAPSendMessage",UTF8);
 if(!a->key_type||!a->key_params||!a->key_data||!a->cmd)goto bad;
 a->bound=1;return 1;
bad:mu_inner_ap_unbind(a);return -1;
}
int mu_inner_ap_send_with_completion(MuInnerAirplay *a,const unsigned char *p,size_t n,
 MuInnerAirplayCompletion done,void *ctx){
 if(!live(a)||!p||!n||n>MU_INNER_CAP||(!done&&ctx))return -1;
 void *d=a->dict(NULL,0,a->key_callbacks,a->value_callbacks);
 void *v=a->dict(NULL,0,a->key_callbacks,a->value_callbacks);
 int r=-1;
 if(d&&v&&a->set_string(d,a->key_type,"iAPSendMessage",14)==0&&
    a->set_data(v,a->key_data,p,(long)n)==0){
  a->set(d,a->key_params,v);
  if(a->get(d,a->key_params)==v&&live(a)){
   /* Stock 4-argument API serializes synchronously then enqueues HTTP.
    * Do not retry an error of uncertain side effects, or fabricate an ACK. */
   if(a->send(a->session,d,done,ctx)==0)r=1;
  }
 }
 if(v)a->release(v);
 if(d)a->release(d);
 return r;
}
int mu_inner_ap_send(void *ctx,const unsigned char *p,size_t n){
 return mu_inner_ap_send_with_completion(ctx,p,n,NULL,NULL);
}
/* The OEM split callback and decoded wire envelope share the same parser.
 * A caller must independently prove that it owns the decoded control
 * dispatch and can propagate the return status to the peer. */
int mu_inner_ap_control_parts(MuInnerAirplay *a,MuInner *b,uint64_t e,
                             const void *session,const void *t,const void *v){
 if(!live(a)||!b||e!=a->epoch||session!=a->session)return -1;
 if(!t||a->type(t)!=a->string_type())return -1;
 if(!a->equal(t,a->cmd))return 2;
 if(!v||a->type(v)!=a->dict_type())return -1;
 const void *data=a->get(v,a->key_data);
 if(!data||a->type(data)!=a->data_type())return -1;
 long n=a->length(data);const unsigned char *p=a->bytes(data);
 if(n<=0||n>(long)MU_INNER_CAP||!p)return -1;
 return mu_inner_from_control(b,e,p,(size_t)n);
}
int mu_inner_ap_control(MuInnerAirplay *a,MuInner *b,uint64_t e,const void *d){
 if(!live(a)||!b||e!=a->epoch||!d||a->type(d)!=a->dict_type())return -1;
 const void *t=a->get(d,a->key_type);
 /* Preserve the existing envelope adapter's unrelated-message contract. */
 if(!t||a->type(t)!=a->string_type()||!a->equal(t,a->cmd))return 2;
 return mu_inner_ap_control_parts(a,b,e,a->session,t,a->get(d,a->key_params));
}
