/* SPDX-License-Identifier: GPL-3.0-only */
#include "inner_iap2.h"
#include <string.h>
static const unsigned char detect[]={0xff,0x55,0x02,0x00,0xee,0x10};
static int sum_ok(const unsigned char *p,size_t n){
 unsigned s=0;for(size_t i=0;i<n;++i)s+=p[i];return !(s&255u);
}
/* Return 0 incomplete, -1 malformed, otherwise exact full frame length.
 * Forward original bytes: no synthesized auth, ACK, or session IDs. */
static int packet(const unsigned char *p,size_t n){
 if(!n)return 0;
 if(p[0]!=0xff)return -1;
 if(n<2)return 0;
 if(p[1]==0x55){
  size_t k=n<sizeof detect?n:sizeof detect;
  if(memcmp(p,detect,k))return -1;
  return n<sizeof detect?0:(int)sizeof detect;
 }
 if(p[1]!=0x5a)return -1;
 if(n<9)return 0;
 size_t z=((size_t)p[2]<<8)|p[3];
 if(z<9||z>MU_INNER_CAP||!sum_ok(p,9))return -1;
 if(n<z)return 0;
 if(z>9&&!sum_ok(p+9,z-9))return -1;
 return (int)z;
}
void mu_inner_end(MuInner *b){
 if(b)memset(b,0,sizeof *b);
}
static int live(MuInner *b,uint64_t e){
 if(!b||!b->active||b->epoch!=e)return 0;
 if(b->ops.owner(b->ops.ctx,e)!=1){mu_inner_end(b);return 0;}
 return 1;
}
int mu_inner_start(MuInner *b,uint64_t e,const MuInnerOps *o){
 if(!b||b->active||!e||!o||!o->owner||!o->send_event||!o->write_cinemo||o->owner(o->ctx,e)!=1)return -1;
 mu_inner_end(b);b->ops=*o;b->epoch=e;b->active=1;return 1;
}
int mu_inner_from_cinemo(MuInner *b,uint64_t e,const unsigned char *p,size_t n){
 if(!live(b,e)||!p||!n||n>MU_INNER_CAP)return -1;
 if(n>MU_INNER_CAP-b->tx_len)return 0;
 memcpy(b->tx+b->tx_len,p,n);b->tx_len+=n;return 1;
}
int mu_inner_from_control(MuInner *b,uint64_t e,const unsigned char *p,size_t n){
 if(!live(b,e)||!p||!n||n>MU_INNER_CAP)return -1;
 if(packet(p,n)!=(int)n){mu_inner_end(b);return -1;}
 if(b->rx_len)return 0;
 memcpy(b->rx,p,n);b->rx_len=n;b->rx_off=0;return 1;
}
int mu_inner_pump(MuInner *b,uint64_t e){
 if(!live(b,e))return -1;
 if(b->rx_len){
  size_t left=b->rx_len-b->rx_off;
  int r=b->ops.write_cinemo(b->ops.ctx,b->rx+b->rx_off,left);
  if(r<0||(size_t)r>left){mu_inner_end(b);return -1;}
  b->rx_off+=(size_t)r;
  if(b->rx_off==b->rx_len)b->rx_off=b->rx_len=0;
 }
 if(!live(b,e))return -1;
 int n=packet(b->tx,b->tx_len);
 if(n<0){mu_inner_end(b);return -1;}
 if(n){
  int r=b->ops.send_event(b->ops.ctx,b->tx,(size_t)n);
  if(r!=0&&r!=1){mu_inner_end(b);return -1;}
  if(r==1){b->tx_len-=(size_t)n;memmove(b->tx,b->tx+n,b->tx_len);}
 }
 return 1;
}
