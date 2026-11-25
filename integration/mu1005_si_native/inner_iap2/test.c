/* SPDX-License-Identifier: GPL-3.0-only */
#include "inner_iap2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static MuInner b;
static unsigned char det[]={255,85,2,0,238,16};
static unsigned char ack[]={255,90,0,9,64,0,0,0,94};
static struct {int live,busy,events;size_t n;unsigned char bytes[64];} x;
static int owner(void *p,uint64_t e){(void)p;return x.live&&e==7;}
static int event(void *p,const unsigned char *v,size_t n){
 (void)p;if(x.busy)return 0;
 if(x.events==0){assert(n==sizeof det&&!memcmp(v,det,n));}
 else {assert(n==sizeof ack&&!memcmp(v,ack,n));}
 x.events++;return 1;
}
static int wr(void *p,const unsigned char *v,size_t n){
 (void)p;if(n>2)n=2;memcpy(x.bytes+x.n,v,n);x.n+=n;return (int)n;
}
int main(void){
 MuInnerOps o={0,owner,event,wr};x.live=1;
 assert(mu_inner_start(&b,7,&o)==1);
 assert(mu_inner_from_cinemo(&b,7,det,2)==1);
 assert(mu_inner_pump(&b,7)==1&&x.events==0);
 assert(mu_inner_from_cinemo(&b,7,det+2,4)==1);
 assert(mu_inner_from_cinemo(&b,7,ack,sizeof ack)==1);
 x.busy=1;assert(mu_inner_pump(&b,7)==1&&x.events==0);
 x.busy=0;assert(mu_inner_pump(&b,7)==1&&x.events==1);
 assert(mu_inner_pump(&b,7)==1&&x.events==2);
 assert(mu_inner_from_control(&b,7,ack,sizeof ack)==1);
 assert(mu_inner_from_control(&b,7,det,sizeof det)==0);
 for(int i=0;i<5;i++)assert(mu_inner_pump(&b,7)==1);
 assert(x.n==sizeof ack&&!memcmp(x.bytes,ack,x.n));
 assert(mu_inner_pump(&b,6)==-1&&b.active);
 assert(mu_inner_from_control(&b,7,det,sizeof det)==1);
 x.live=0;assert(mu_inner_pump(&b,7)==-1&&!b.active);
 x.live=1;assert(mu_inner_start(&b,7,&o)==1);
 ack[8]^=1;assert(mu_inner_from_control(&b,7,ack,sizeof ack)==-1&&!b.active);
 puts("INNER_IAP2_PACKET_TRANSPORT_HOST_PASS_NO_OEM");return 0;
}
