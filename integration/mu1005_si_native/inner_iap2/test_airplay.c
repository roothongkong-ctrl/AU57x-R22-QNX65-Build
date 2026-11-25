/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only ABI-shaped fake CFL library: checks adapter behavior,
 * not OEM ABI, runtime or authorization. */
#include "inner_airplay.h"
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
int AirPlayReceiverSessionSendCommand(void *s,const void *d,void(*done)(void),void *ctx){
 assert(s==&session&&!done&&!ctx);
 Obj *o=(Obj*)d;assert(o->type==DICT);
 assert(!strcmp(o->field[0]->str,"iAPSendMessage"));
 Obj *v=o->field[1];assert(v&&v->type==DICT&&v->field[2]->type==DATA);
 assert(v->field[2]->n==6&&!memcmp(v->field[2]->data,marker,6));
 last=o;sends++;return fail_send?-1:0;
}
static int library(void *p,void *h){(void)p;return h==handle;}
static int owner(void *p,void *s,uint64_t e){(void)p;return owner_live&&s==&session&&e==7;}
static int relay_owner(void *p,uint64_t e){(void)p;return owner_live&&e==7;}
static int write_cinemo(void *p,const unsigned char *b,size_t n){
 (void)p;assert(n==6&&!memcmp(b,marker,n));return (int)n;
}
int main(void){
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
 Obj other={.type=STR,.str="otherCommand"};
 Obj *old=last->field[0];last->field[0]=&other;
 assert(mu_inner_ap_control(&a,&relay,7,last)==2);
 last->field[0]=old;
 Obj *params=last->field[1];last->field[1]=NULL;
 assert(mu_inner_ap_control(&a,&relay,7,last)==-1);last->field[1]=params;
 fail_send=1;assert(mu_inner_from_cinemo(&relay,7,marker,6)==1);
 assert(mu_inner_pump(&relay,7)==-1&&!relay.active&&sends==2);
 owner_live=0;assert(mu_inner_ap_send(&a,marker,6)==-1&&sends==2);
 mu_inner_ap_unbind(&a);assert(!a.bound);
 dlclose(handle);puts("MU1005_INNER_AIRPLAY_ENVELOPE_HOST_PASS_NO_OEM");return 0;
}
