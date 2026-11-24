#include "r129_driver_call.h"
#include <dlfcn.h>
#include <string.h>
/* Pinned ARM32 device offsets from MU1005 mm-ipod and stock USB transport.
 * On a host these are exercised only via an explicit synthetic reader. */
static int slot(R129Driver *d,void *base,unsigned off,uintptr_t *out){
 uintptr_t a=(uintptr_t)base;
 return a&&a<=UINTPTR_MAX-off&&d->proof.read_live(d->proof.context,a+off,out,sizeof *out)==1;
}
static int live(R129Driver *d,void *device){
 uintptr_t drv,owner,start,recv,opts,expect_start=0,expect_recv=0;
 if(!d||!d->bound||device!=d->device||d->proof.certify(d->proof.context,device,d->driver,d->handle)!=1)return 0;
 memcpy(&expect_start,&d->start,sizeof expect_start);memcpy(&expect_recv,&d->receive,sizeof expect_recv);
 return slot(d,device,0x38,&drv)&&drv==(uintptr_t)d->driver&&
 slot(d,d->driver,0x14,&owner)&&owner==(uintptr_t)device&&
 slot(d,d->driver,0x18,&start)&&start==expect_start&&
 slot(d,d->driver,0x20,&recv)&&recv==expect_recv&&
 slot(d,device,0x0c,&opts)&&opts==d->options;
}
static int owner_ok(void *p,void *dev){return live(p,dev);}
static int auth(void *p,void *dev,uint32_t generation){R129Driver*d=p;return live(d,dev)&&generation&&d->proof.authorize(d->proof.context,dev,generation)==1;}
static int start(void *p,void *dev){R129Driver*d=p;if(!live(d,dev)||d->started)return -1;
 /* iap2_init uses getsubopt: give it a writable, resident copy of the
  * actual driver options string, NOT the device object pointer. */
 size_t i;for(i=0;i<sizeof d->opts;i++){
  if(d->options>UINTPTR_MAX-i||d->proof.read_live(d->proof.context,d->options+i,d->opts+i,1)!=1)return -1;
  if(!d->opts[i])break;
 }
 if(i==sizeof d->opts||!i||!live(d,dev))return -1;
 d->started=1;return d->start(d->driver,d->opts);
}
static void input(void *p,void *dev){R129Driver*d=p;if(live(d,dev)&&d->started)d->last_input=d->receive(d->driver);}
static int stop(void *p,void *dev){R129Driver*d=p;if(!live(d,dev))return -1;return d->proof.teardown(d->proof.context,dev);}
int r129_driver_bind(R129Driver*d,void *device,void *handle,const R129Evidence*p,R125Owner*out){
 if(!d||d->bound||!device||!handle||!p||!out||!p->certify||!p->read_live||!p->authorize||!p->teardown)return -1;
 if(sizeof d->start!=sizeof(uintptr_t)||sizeof d->receive!=sizeof(uintptr_t))return -1;
 memset(out,0,sizeof *out);memset(d,0,sizeof *d);d->proof=*p;d->device=device;d->handle=handle;
 /* The external provider must certify the device before any slot read. */
 if(p->certify(p->context,device,NULL,handle)!=1)goto fail;
 uintptr_t drv=0;if(!slot(d,device,0x38,&drv)||!drv||!slot(d,device,0x0c,&d->options)||!d->options)goto fail;
 d->driver=(void*)drv;if(p->certify(p->context,device,d->driver,handle)!=1)goto fail;
 void *s=dlsym(handle,"iap2_init"),*r=dlsym(handle,"transport_recv_pkt");if(!s||!r)goto fail;
 memcpy(&d->start,&s,sizeof s);memcpy(&d->receive,&r,sizeof r);d->bound=1;
 if(!live(d,device))goto fail;
 *out=(R125Owner){d,owner_ok,auth,start,input,stop};return 0;
fail: memset(d,0,sizeof *d);return -1;
}
