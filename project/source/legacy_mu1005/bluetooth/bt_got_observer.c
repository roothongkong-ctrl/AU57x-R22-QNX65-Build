/* SPDX-License-Identifier: GPL-3.0-only
 * R97 opt-in passive hook for ORIGINAL HFP ONLY. This does not register a new
 * Apple iAP2 RFCOMM channel or prove native Bluetooth callback permissions.
 */
#include "bt_got_observer.h"
#include "bt_thread_owner.h"
#include <string.h>
#include <stdint.h>

static Au97GotHook hfp_hook;
static volatile unsigned event_seen[7],event_on_observed,event_on_unknown,active;
typedef void (*Au97Callback)(void *,const void *);

int au97_got_replace(Au97GotHook *hook,volatile uintptr_t *slot,
                     uintptr_t original,uintptr_t replacement){
    if(!hook||!slot||!original||!replacement||original==replacement ||
       hook->installed || ((uintptr_t)slot&(sizeof(uintptr_t)-1)))return -1;
    /* Record original function before publication of the wrapper pointer. */
    hook->original=original;
    hook->replacement=replacement;
    hook->slot=slot;
    if(!__sync_bool_compare_and_swap(slot,original,replacement)) {
        hook->slot=NULL;
        hook->original=hook->replacement=0;
        return -1;
    }
    __sync_synchronize();
    hook->installed=1;
    return 0;
}
int au97_got_restore_quiesced(Au97GotHook *hook,int owner_drained){
    if(!hook||!owner_drained||hook->installed!=1||!hook->slot||
       __sync_fetch_and_add(&active,0u)!=0)return -1;
    /* Caller must also have revoked the OEM's persisted callback pointers.
     * The GOT alone is NOT proof an existing RFCOMM service no longer has
     * the wrapper address cached. Not used from process exit/destructor. */
    if(!__sync_bool_compare_and_swap(hook->slot,hook->replacement,hook->original))return -1;
    __sync_synchronize();
    hook->installed=0;
    return 0;
}
static void __attribute__((unused)) hfp_observer(void *channel,const void *event){
    Au97Callback next=NULL;
    uintptr_t orig=hfp_hook.original;
    unsigned char code=0;
    __sync_fetch_and_add(&active,1u);
    if(event){
        code=*(const volatile unsigned char*)event;
        if(code>=1&&code<=6)__sync_fetch_and_add(&event_seen[code],1u);
        if(au97_current_observed_thread())__sync_fetch_and_add(&event_on_observed,1u);
        else __sync_fetch_and_add(&event_on_unknown,1u);
    }
    /* Portable C pointer bitcopy to preserve native 32-bit callable ABI. */
    if(orig){memcpy(&next,&orig,sizeof next);next(channel,event);}
    __sync_fetch_and_sub(&active,1u);
}
#if defined(AU57X_R97_HOST_TEST)
int au97_test_install_hfp(volatile uintptr_t *slot,uintptr_t original){
    return au97_got_replace(&hfp_hook,slot,original,(uintptr_t)&hfp_observer);
}
#endif
int au97_install_original_hfp_observer(void){
#if defined(__QNXNTO__) && defined(__arm__) && UINTPTR_MAX == UINT32_MAX
    /* SHA pinned R92 original MU1005 btstack GLOB_DAT symbol (not text patch):
     * 0x2c23c8 R_ARM_GLOB_DAT HfRfCallback, defined at 0x2481f0.
     * 0x24648c reads the relocated GOT value; subsequent 0x2464ac stores
     * the original stock callback before RF registration at 0x2464b0.
     * Requires preloading before original HFP service registration. */
    return au97_got_replace(&hfp_hook,(volatile uintptr_t*)0x002c23c8u,
           0x002481f0u,(uintptr_t)&hfp_observer);
#else
    return -1;
#endif
}
void au97_hfp_observer_stats(Au97GotStats*out){
    unsigned i;
    if(!out)return;
    for(i=0;i<7;i++)out->event_seen[i]=__sync_fetch_and_add(&event_seen[i],0u);
    out->on_observed_thread=__sync_fetch_and_add(&event_on_observed,0u);
    out->on_unknown_thread=__sync_fetch_and_add(&event_on_unknown,0u);
    out->active_callbacks=__sync_fetch_and_add(&active,0u);
}
