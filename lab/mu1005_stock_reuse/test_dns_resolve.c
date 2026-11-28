#include "dns_resolve.h"
#include <assert.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
typedef void(*cb_t)(void*,uint32_t,uint32_t,int32_t,const char*,const char*,uint16_t,uint16_t,const unsigned char*,void*);
static int p[2],mode,alive=1,disposed;
static cb_t cb;static void *context;
static int live(void*v){(void)v;return alive;}
int32_t DNSServiceResolve(void**ref,uint32_t flags,uint32_t index,const char*instance,const char*type,const char*domain,cb_t f,void*v){
 assert(!flags&&index==7&&!strcmp(instance,"OEM")&&!strcmp(type,"_airplay._tcp")&&!strcmp(domain,"local."));
 if(mode==5)return -1;
 assert(pipe(p)==0);*ref=p;cb=f;context=v;return 0;
}
int DNSServiceRefSockFD(void*r){assert(r==p);return p[0];}
int32_t DNSServiceProcessResult(void*r){
 char b;assert(r==p&&read(p[0],&b,1)==1);
 if(mode==4)return -1;
 cb(r,0,mode==1?8:7,mode==6?-1:0,"OEM._airplay._tcp.local.",
 mode==2?"foreign.local.":"oem.local.",htons(mode==3?7001:7000),0,NULL,context);
 if(mode==7)alive=0;
 return 0;
}
void DNSServiceRefDeallocate(void*r){assert(r==p);close(p[0]);close(p[1]);disposed++;}
int main(void){
 void*h=dlopen(NULL,RTLD_NOW);assert(h);
 for(mode=0;mode<=7;mode++){
  alive=1;
  mu_dns_witness*w=mu_dns_begin(h,live,NULL,7,"OEM","_airplay._tcp","local.","OEM._airplay._tcp.local.","oem.local.",7000);
  if(mode==5){assert(!w);continue;}
  assert(w&&mu_dns_poll(w)==0);
  assert(write(p[1],"x",1)==1);
  assert(mu_dns_poll(w)==(mode==0?1:-1));
  if(mode==0){alive=0;assert(mu_dns_poll(w)==-1);}
  mu_dns_end(w);
 }
 assert(disposed==7);dlclose(h);puts("DNS_WITNESS_8_CASES_PASS_NO_REGISTRATION");
}
