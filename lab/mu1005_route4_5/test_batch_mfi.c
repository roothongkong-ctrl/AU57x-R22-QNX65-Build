/* Synthetic provider, no original MFi credentials or device access. */
#include "cp_step04_stock_mfi.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int live=1,mode,freed;
static int valid(void*p,uint32_t e,uint32_t g){(void)p;return live&&e==1&&g==2;}
static int stock(void*p){(void)p;return live;}
static int produce(uint8_t**p,size_t*n){
 *n=20;*p=malloc(*n);assert(*p);memset(*p,0x41,*n);
 if(mode==0||mode==1)live=0;
 return 0;
}
int APSMFiPlatform_CopyCertificate(uint8_t**p,size_t*n){return produce(p,n);}
int APSMFiPlatform_CreateSignature(const uint8_t*c,size_t l,uint8_t**p,size_t*n){assert(c&&l==20);return produce(p,n);}
static void drop(void*p,void*v){(void)p;free(v);freed++;if(mode==2||mode==3)live=0;}
int main(int argc,char**argv){
 assert(argc==2);mode=atoi(argv[1]);assert(mode>=0&&mode<=3);
 struct cp_stock_mfi s;struct cp_stock_owner o={0,valid,stock,valid,stock,drop};
 void*h=dlopen(NULL,RTLD_NOW);assert(h);
 assert(cp_stock_mfi_bind(&s,h,&o,1,2)==0);
 uint8_t b[64],c[20]={0};size_t n=99;memset(b,0xcc,sizeof b);
 int r=(mode%2)?cp_stock_mfi_sign(&s,c,sizeof c,b,sizeof b,&n):cp_stock_mfi_cert(&s,b,sizeof b,&n);
 assert(r==CP_STOCK_OWNER&&n==0&&freed==1&&!s.bound);
 for(unsigned i=0;i<20;i++)assert(b[i]==(mode>=2?0:0xcc));
 live=1;assert(!cp_stock_mfi_live(&s));dlclose(h);
 printf("MFI_REVOKE_CASE_%d_PASS_FIXTURE_ONLY\n",mode);
}
