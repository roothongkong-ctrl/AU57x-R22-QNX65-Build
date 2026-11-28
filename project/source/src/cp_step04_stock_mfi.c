/* SPDX-License-Identifier: GPL-3.0-only
 * Verified symbol names in exact R14 MU1005 sample libairplay.so.
 * Does not itself establish original process identity, MFi authorization or
 * control session ownership. All such facts are external OEM attestations.
 */
#include "cp_step04_stock_mfi.h"
#include <dlfcn.h>
#include <string.h>
static int symbol(void *lib,const char *name,void *out,size_t n){
 void *p;
 if(!lib||!out||n!=sizeof p)return -1;
 (void)dlerror();p=dlsym(lib,name);
 if(!p||dlerror()!=NULL)return -1;
 memcpy(out,&p,sizeof p);return 0;
}
int cp_stock_mfi_live(struct cp_stock_mfi *s){
 return s&&s->bound&&s->existing_library&&s->owner.owner_verified&&
  s->owner.stock_image_verified&&s->owner.control_session_live&&
  s->owner.wired_idle&&s->owner.release_oem&&
  s->owner.owner_verified(s->owner.opaque,s->epoch,s->bt_generation)==1&&
  s->owner.stock_image_verified(s->owner.opaque)==1&&
  s->owner.control_session_live(s->owner.opaque,s->epoch,s->bt_generation)==1&&
  s->owner.wired_idle(s->owner.opaque)==1;
}
int cp_stock_mfi_bind(struct cp_stock_mfi *s,void *lib,
 const struct cp_stock_owner *o,uint32_t epoch,uint32_t gen){
 if(!s||!lib||!o||!epoch||!gen||!o->owner_verified||
    !o->stock_image_verified||!o->control_session_live||!o->wired_idle||
    !o->release_oem)return CP_STOCK_INVALID;
 memset(s,0,sizeof *s);
 s->owner=*o;s->existing_library=lib;s->epoch=epoch;s->bt_generation=gen;
 s->bound=1;
 if(!cp_stock_mfi_live(s)){cp_stock_mfi_invalidate(s);return CP_STOCK_OWNER;}
 if(symbol(lib,"APSMFiPlatform_CopyCertificate",&s->copy_certificate,
           sizeof s->copy_certificate)!=0||
    symbol(lib,"APSMFiPlatform_CreateSignature",&s->create_signature,
           sizeof s->create_signature)!=0){
  cp_stock_mfi_invalidate(s);return CP_STOCK_VENDOR;
 }
 if(!cp_stock_mfi_live(s)){cp_stock_mfi_invalidate(s);return CP_STOCK_OWNER;}
 return CP_STOCK_OK;
}
static void release(struct cp_stock_mfi *s,uint8_t *ptr){
 if(ptr&&s->owner.release_oem)s->owner.release_oem(s->owner.opaque,ptr);
}
/* OEM call may block long enough for wired ownership or generation to
 * change. Release the original allocation once even when revoked; never
 * publish its bytes for a stale session. Serialized owner contract remains. */
static int finish_oem(struct cp_stock_mfi *s,int r,uint8_t *owned,size_t size,
 uint8_t *dst,size_t cap,size_t limit,size_t *n){
 if(!cp_stock_mfi_live(s)){
  release(s,owned);cp_stock_mfi_invalidate(s);return CP_STOCK_OWNER;
 }
 if(r!=0){release(s,owned);return CP_STOCK_VENDOR;}
 if(!owned||!size||size>limit||size>cap){release(s,owned);return CP_STOCK_OVERFLOW;}
 memcpy(dst,owned,size);release(s,owned);
 if(!cp_stock_mfi_live(s)){
  volatile uint8_t *p=dst;for(size_t i=0;i<size;i++)p[i]=0;
  cp_stock_mfi_invalidate(s);return CP_STOCK_OWNER;
 }
 *n=size;return CP_STOCK_OK;
}
int cp_stock_mfi_cert(struct cp_stock_mfi *s,uint8_t *dst,size_t cap,size_t *n){
 uint8_t *owned=NULL;size_t size=0;int r;
 if(n)*n=0;
 if(!s||!dst||!n||!s->copy_certificate)return CP_STOCK_INVALID;
 if(!cp_stock_mfi_live(s))return CP_STOCK_OWNER;
 r=s->copy_certificate(&owned,&size);
 return finish_oem(s,r,owned,size,dst,cap,CP_STOCK_CERT_MAX,n);
}
int cp_stock_mfi_sign(struct cp_stock_mfi *s,const uint8_t *challenge,size_t len,
                      uint8_t *dst,size_t cap,size_t *n){
 uint8_t *owned=NULL;size_t size=0;int r;
 if(n)*n=0;
 if(!s||!dst||!n||!challenge||len!=CP_STOCK_CHALLENGE||
    !s->create_signature)return CP_STOCK_INVALID;
 if(!cp_stock_mfi_live(s))return CP_STOCK_OWNER;
 r=s->create_signature(challenge,len,&owned,&size);
 return finish_oem(s,r,owned,size,dst,cap,CP_STOCK_SIGNATURE_MAX,n);
}
void cp_stock_mfi_invalidate(struct cp_stock_mfi *s){if(s)memset(s,0,sizeof *s);}
