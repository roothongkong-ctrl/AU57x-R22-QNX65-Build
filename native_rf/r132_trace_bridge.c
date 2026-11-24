#include "r132_trace_bridge.h"
#include "r131_trace_entry.h"
/* No mutex, allocation, IO, vendor calls, or logging from trace callback.
 * Permanent binding, with no unload/rearm until original process exits. */
static uintptr_t original;
static uint32_t events,owner_hint,conflict;
int r132_trace_bind(uintptr_t fn){
 uintptr_t expected=0;
 if(!fn)return -1;
 return __atomic_compare_exchange_n(&original,&expected,fn,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED)?0:-1;
}
uintptr_t r132_trace_dispatch(const R132TraceFrame *f){
 uintptr_t fn=__atomic_load_n(&original,__ATOMIC_ACQUIRE);
 if(!fn||!f)return fn;
 unsigned kind=r131_trace_classify(f->lr,f->r0,(int)f->r1,f->r2);
 if(kind==R131_TRACE_DID){
  uint32_t zero=0,a=f->r5;
  if(!a||(a&3)||a>UINT32_MAX-0x1b0u||
   (!__atomic_compare_exchange_n(&owner_hint,&zero,a,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)&&zero!=a))
   __atomic_store_n(&conflict,1,__ATOMIC_RELEASE);
 }
 if(kind)__atomic_fetch_or(&events,kind,__ATOMIC_RELEASE);
 return fn;
}
void r132_trace_take(R132TraceHints *o){
 if(!o)return;
 o->events=__atomic_exchange_n(&events,0,__ATOMIC_ACQ_REL);
 o->owner_hint=__atomic_load_n(&owner_hint,__ATOMIC_ACQUIRE);
 o->conflict=__atomic_load_n(&conflict,__ATOMIC_ACQUIRE);
}
int r132_start_observed(const R132TraceHints *h,const R132Live *v){
 uint8_t a=0,b=0;
 if(!h||!v||!v->certify||!v->read||!h->owner_hint||h->conflict||
    h->owner_hint>UINT32_MAX-0x1b0u||(h->owner_hint&3))return 0;
 if(__atomic_load_n(&conflict,__ATOMIC_ACQUIRE)||__atomic_load_n(&owner_hint,__ATOMIC_ACQUIRE)!=h->owner_hint)return 0;
 uint32_t p=h->owner_hint;
 if(v->certify(v->context,p)!=1||v->read(v->context,p+0x1b0u,&a,1)!=1||a!=1||
    v->certify(v->context,p)!=1||v->read(v->context,p+0x1b0u,&b,1)!=1||b!=1||v->certify(v->context,p)!=1)return 0;
 return !__atomic_load_n(&conflict,__ATOMIC_ACQUIRE)&&__atomic_load_n(&owner_hint,__ATOMIC_ACQUIRE)==p;
}
