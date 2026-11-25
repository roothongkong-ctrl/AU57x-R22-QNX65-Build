/* SPDX-License-Identifier: GPL-3.0-only */
#include "mm_notify.h"
#include <dlfcn.h>
#include <string.h>
#if !defined(MU_MM_NOTIFY_HOST_TEST)
#if !defined(__QNXNTO__) || !defined(__arm__)
#error "MU1005 native layout is only supported on QNX ARM"
#endif
typedef char pointer_must_be_32bit[sizeof(void*)==4?1:-1];
#endif
/* OEM offsets confirmed in device_drvr_init and usbhost_pulse.
 * Host fixture uses pointer-sized slots at the same offsets, not OEM ABI. */
static void *ptr(const void *base,unsigned offset){
 void *p;memcpy(&p,(const unsigned char *)base+offset,sizeof p);return p;
}
static int live(MuMmNotify *n){
 if(!n||!n->bound||!n->owner||
    n->owner(n->ctx,n->device,n->driver,n->transport,n->epoch)!=1)return 0;
 if(ptr(n->device,0x38)!=n->driver||ptr(n->device,0x40)!=n->transport||
    ptr(n->driver,0x14)!=n->device)return 0;
 MuMmDriverRead slot;
 memcpy(&slot,(const unsigned char *)n->driver+0x20,sizeof slot);
 return slot==n->read;
}
void mu_mm_notify_clear(MuMmNotify *n){
 if(n&&!n->calling)memset(n,0,sizeof *n);
}
int mu_mm_notify_bind(MuMmNotify *n,void *h,void *dev,void *driver,void *transport,
 uint64_t epoch,void *ctx,MuMmNotifyProof owner){
 if(!n||n->bound||!h||!dev||!driver||!transport||!epoch||!owner||
    owner(ctx,dev,driver,transport,epoch)!=1)return -1;
 (void)dlerror();void *p=dlsym(h,"transport_recv_pkt");
 if(!p||dlerror()||sizeof p!=sizeof n->read)return -1;
 memset(n,0,sizeof *n);n->handle=h;n->device=dev;n->driver=driver;
 n->transport=transport;n->epoch=epoch;n->ctx=ctx;n->owner=owner;
 memcpy(&n->read,&p,sizeof p);n->bound=1;
 if(!live(n)){mu_mm_notify_clear(n);return -1;}return 1;
}
int mu_mm_notify_input(void *ctx,void *device){
 MuMmNotify *n=ctx;
 if(!n||device!=n->device||n->calling||!live(n))return -1;
 /* Exact stock usbhost_pulse event 3 ABI: one arg is driver, not device.
  * Original caller ignores the status; driver owns parse and pool handling.
  * Calling owner must monitor native link state independently. */
 n->calling=1;(void)n->read(n->driver);n->calling=0;
 return live(n)?1:-1;
}
