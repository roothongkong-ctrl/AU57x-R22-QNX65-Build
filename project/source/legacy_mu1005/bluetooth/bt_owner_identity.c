#include "bt_owner_identity.h"
#include <string.h>
static uint32_t le(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int object(uint32_t a,uint32_t max){return a&&!(a&3u)&&a<=UINT32_MAX-max;}
typedef struct {uint8_t os[0x40],factory[12],ao[0xf4];} Snap;
static int take(Snap*s,uint32_t bias,uint32_t tid,int(*r)(void*,uint32_t,void*,size_t),void*c){
 if(r(c,0x2d5644u,s->os,sizeof s->os)!=1||le(s->os)!=0x2c10d8u||s->os[0x16]!=1)return 0;
 uint32_t f=le(s->os+0x3c);
 if(!object(f,11)||r(c,f,s->factory,sizeof s->factory)!=1||le(s->factory)!=0x2c1098u||!object(le(s->factory+4),3))return 0;
 uint32_t a=le(s->factory+8);
 if(!object(a,0xf3)||r(c,a,s->ao,sizeof s->ao)!=1)return 0;
 return le(s->ao)==bias+0xc69a8u&&s->ao[4]==1u&&le(s->ao+0xf0)==0&&le(s->ao+0x20)==tid;
}
int mu_bt_osapi_thread_matches(uint32_t bias,uint32_t tid,int(*r)(void*,uint32_t,void*,size_t),void*c){
 Snap a,b;
 if(!r||!bias||(bias&0xfffu)||bias>UINT32_MAX-0xc69a8u||!tid||tid==UINT32_MAX)return 0;
 /* Do not compare changing queue contents: compare only identity/lifetime
  * fields, not the internal linked list or synchronization object bytes. */
 if(!take(&a,bias,tid,r,c)||!take(&b,bias,tid,r,c))return 0;
 return !memcmp(a.os,b.os,4)&&a.os[0x16]==b.os[0x16]&&
  !memcmp(a.os+0x3c,b.os+0x3c,4)&&!memcmp(a.factory,b.factory,12)&&
  !memcmp(a.ao,b.ao,5)&&!memcmp(a.ao+0x20,b.ao+0x20,4)&&!memcmp(a.ao+0xf0,b.ao+0xf0,4);
}
