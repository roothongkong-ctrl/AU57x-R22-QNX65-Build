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
int mu_bt_comm_words_match(uint32_t bias,int(*r)(void*,uint32_t,void*,size_t),void*c){
 static const uint32_t anchors[][2]={{0x4d878u,0xe2800008u},{0x4d87cu,0xebff1d80u},
  {0x4e620u,0xe59030f0u},{0x4e9d0u,0xe280a0c8u},{0x4ea74u,0xebff1b78u},{0x4eab4u,0xe12fff33u}};
 uint8_t b[4];
 if(!r||!bias||(bias&0xfffu)||bias>UINT32_MAX-0xc69a8u)return 0;
 for(size_t i=0;i<sizeof anchors/sizeof anchors[0];++i)
  if(r(c,bias+anchors[i][0],b,4)!=1||le(b)!=anchors[i][1])return 0;
 return 1;
}
int mu_bt_osapi_identity(uint32_t bias,uint32_t tid,int(*r)(void*,uint32_t,void*,size_t),void*c,MuBtThreadIdentity*out){
 Snap a,b;
 if(!out)return 0;
 memset(out,0,sizeof *out);
 if(!r||!bias||(bias&0xfffu)||bias>UINT32_MAX-0xc69a8u||!tid||tid==UINT32_MAX)return 0;
 /* Do not compare changing queue contents: compare only identity/lifetime
  * fields, not the internal linked list or synchronization object bytes. */
 if(!take(&a,bias,tid,r,c)||!take(&b,bias,tid,r,c))return 0;
 if(memcmp(a.os,b.os,4)||a.os[0x16]!=b.os[0x16]||
  memcmp(a.os+0x3c,b.os+0x3c,4)||memcmp(a.factory,b.factory,12)||
  memcmp(a.ao,b.ao,5)||memcmp(a.ao+0x20,b.ao+0x20,4)||memcmp(a.ao+0xf0,b.ao+0xf0,4))return 0;
 *out=(MuBtThreadIdentity){bias,tid,le(a.os+0x3c),le(a.factory+4),le(a.factory+8)};
 return 1;
}
int mu_bt_osapi_identity_matches(const MuBtThreadIdentity*saved,uint32_t tid,int(*r)(void*,uint32_t,void*,size_t),void*c){
 MuBtThreadIdentity current;
 if(!saved||tid!=saved->tid||!mu_bt_osapi_identity(saved->comm_bias,tid,r,c,&current))return 0;
 return current.factory==saved->factory&&current.shared_control==saved->shared_control&&current.active_object==saved->active_object;
}
int mu_bt_osapi_thread_matches(uint32_t bias,uint32_t tid,int(*r)(void*,uint32_t,void*,size_t),void*c){
 MuBtThreadIdentity id;return mu_bt_osapi_identity(bias,tid,r,c,&id);
}
