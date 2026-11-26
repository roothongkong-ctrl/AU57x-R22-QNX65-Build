#include "native_event.h"
#include <string.h>
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int mac_ok(const uint8_t *p){unsigned any=0,all=255;for(int i=0;i<6;i++){any|=p[i];all&=p[i];}return any&&all!=255;}
int mu38_resolve_peer(void *ctx,const void *ch,const void *raw,uint8_t out[6]){
 const Mu38PeerOps *o=ctx;const uint8_t *e=raw;uint8_t a[6]={0};uint32_t p;
 if(!out)return 0;
 memset(out,0,6);
 if(!o||!ch||!e||!o->association_matches||!o->read_live||!o->paired_authorized)return 0;
 if((e[0]!=1&&e[0]!=2)||le16(e+2)!=0)return 0;
 p=le32(e+12);if(!p||(p&3)||p>UINT32_MAX-18u)return 0;
 if(o->association_matches(o->opaque,ch,p,e[0])!=1)return 0;
 if(o->read_live(o->opaque,p+13u,a,6)!=1||!mac_ok(a))return 0;
 if(o->paired_authorized(o->opaque,p,a)!=1)return 0;
 memcpy(out,a,6);return 1;
}
int mu38_dispatch(InboundR5 *b,const Mu38PeerOps *o,void *ch,const void *raw){
 uint8_t e[16]; unsigned code,status,n=0;uint32_t p;int r;
 if(!b||!b->initialized||!o||o!=b->vendor.opaque||!raw||ch!=&b->service.rf||!b->vendor.certify||b->vendor.certify(b->vendor.opaque)!=1)return -1;
 memcpy(e,raw,sizeof e);code=e[0];status=le16(e+2);p=le32(e+12);
 switch(code){
 case 1:case 2:case 4:return ir5_event(b,ch,e,code,status,NULL,0,0);
 case 5:
  n=le16(e+8);if(status||!n||n>MAX_DATA||!p||p>UINT32_MAX-(n-1)||!o->read_live||b->native_rx_busy){
   /* A missing payload means the iAP2 byte stream can no longer be intact. */
   if(b->service.state==IN_LINK){b->service.state=IN_DIRTY;b->dead=1;au_ipc_socket_lost(&b->ipc);}return -1;
  }
  b->native_rx_busy=1;
  if(o->read_live(o->opaque,p,b->native_rx,n)!=1){b->native_rx_busy=0;if(b->service.state==IN_LINK){b->service.state=IN_DIRTY;b->dead=1;au_ipc_socket_lost(&b->ipc);}return -1;}
  r=ir5_event(b,ch,e,code,status,b->native_rx,n,0);b->native_rx_busy=0;return r;
 case 6:return ir5_event(b,ch,e,code,status,NULL,0,p);
 default:return -1;
 }
}

/* Original ET_EXEC bt symbol 0x2d58d4, size 0x1bf26c;
 * GOT 0x2c1964 resolves to bt. See STEP01 native registry evidence. */
typedef struct { uint32_t base,member,association; uint8_t link,slot,flags; } Mu38Registry;
static int registry_read(uint32_t ch,int (*r)(void*,uint32_t,void*,size_t),void *ctx,Mu38Registry *v){
 uint8_t b[4],indices[2];
 if(!ch||(ch&3u)||ch>UINT32_MAX-0x5eu||
    r(ctx,0x2c1964u,b,4)!=1||le32(b)!=0x2d58d4u||
    r(ctx,ch+0x5du,indices,2)!=1||indices[0]>3u||indices[1]>16u)return 0;
 v->base=le32(b);v->link=indices[0];v->slot=indices[1];
 uint32_t link=v->base+(uint32_t)v->link*0xaf8u;
 uint32_t row=link+((uint32_t)v->slot+1u)*16u;
 if(r(ctx,row+0x1bc5ccu,b,4)!=1||le32(b)!=ch)return 0;
 v->member=le32(b);
 if(r(ctx,row+0x1bc5c8u,&v->flags,1)!=1||!(v->flags&1u)||
    r(ctx,link+0x1bc6ecu,b,4)!=1)return 0;
 v->association=le32(b);
 return v->association!=0;
}
int mu38_registry_association(uint32_t ch,uint32_t association,
 int (*r)(void*,uint32_t,void*,size_t),void *ctx){
 Mu38Registry a,b;
 if(!r||!association||(association&3u)||
    !registry_read(ch,r,ctx,&a)||!registry_read(ch,r,ctx,&b))return 0;
 return a.base==b.base&&a.link==b.link&&a.slot==b.slot&&
  a.member==b.member&&(a.flags&1u)==(b.flags&1u)&&
  a.association==association&&b.association==association;
}
