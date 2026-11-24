#include "r129_driver_call.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
static int dev,drv,cert=1,authorized=1,starts,reads,stops;static char options[]="config=/etc/mm/iap2.cfg,probe";
static uintptr_t start_slot,recv_slot;static int wrong;
int iap2_init(void *d,char *o){assert(d==&drv&&o!=options&&!strcmp(o,options));starts++;o[0]='X';return 0;}
int transport_recv_pkt(void *d){assert(d==&drv);reads++;return 7;}
static int prove(void*c,void*d,void*f,void*h){return c==&dev&&d==&dev&&(!f||f==&drv)&&h&&cert;}
static int read_slot(void*c,uintptr_t a,void*out,size_t n){assert(c==&dev);uintptr_t v=0;
 if(n==1&&a>=(uintptr_t)options&&a<(uintptr_t)options+sizeof options){memcpy(out,(void*)a,1);return 1;}
 if(n!=sizeof v)return 0;
 if(a==(uintptr_t)&dev+0x38)v=(uintptr_t)&drv;else if(a==(uintptr_t)&dev+0x0c)v=(uintptr_t)options;
 else if(a==(uintptr_t)&drv+0x14)v=(uintptr_t)&dev;else if(a==(uintptr_t)&drv+0x18)v=wrong?0:start_slot;else if(a==(uintptr_t)&drv+0x20)v=recv_slot;else return 0;
 memcpy(out,&v,n);return 1;}
static int auth(void*c,void*d,uint32_t g){return c==&dev&&d==&dev&&g==7&&authorized;}
static int stop(void*c,void*d){assert(c==&dev&&d==&dev);stops++;return 0;}
int main(void){void*h=dlopen(NULL,RTLD_NOW);assert(h);void*s=dlsym(h,"iap2_init"),*r=dlsym(h,"transport_recv_pkt");assert(s&&r);memcpy(&start_slot,&s,sizeof s);memcpy(&recv_slot,&r,sizeof r);
 R129Evidence p={&dev,prove,read_slot,auth,stop};R129Driver d={0};R125Owner o={0};wrong=1;assert(r129_driver_bind(&d,&dev,h,&p,&o)==-1&&!d.bound);wrong=0;cert=0;assert(r129_driver_bind(&d,&dev,h,&p,&o)==-1);cert=1;assert(!r129_driver_bind(&d,&dev,h,&p,&o));assert(o.authorized(o.context,&dev,7));authorized=0;assert(!o.authorized(o.context,&dev,7));authorized=1;
 assert(!o.start_driver(o.context,&dev)&&starts==1&&!strcmp(options,"config=/etc/mm/iap2.cfg,probe"));assert(o.start_driver(o.context,&dev)==-1);o.input_ready(o.context,&dev);assert(reads==1&&d.last_input==7);wrong=1;o.input_ready(o.context,&dev);assert(reads==1&&o.stop_driver(o.context,&dev)==-1);wrong=0;assert(!o.stop_driver(o.context,&dev)&&stops==1);dlclose(h);puts("R129 driver call ABI fixture PASS; no OEM execution");return 0;}
