#ifndef AU57X_R97_BT_GOT_OBSERVER_H
#define AU57X_R97_BT_GOT_OBSERVER_H
#include <stdint.h>
/* A single imported original HFP callback pointer. NOT iAP2/RFCOMM enrollment. */
typedef struct {
    volatile uintptr_t *slot;
    uintptr_t original;
    uintptr_t replacement;
    volatile unsigned installed;
} Au97GotHook;
typedef struct {
    unsigned event_seen[7];
    unsigned on_observed_thread;
    unsigned on_unknown_thread;
    unsigned active_callbacks;
} Au97GotStats;
/* Caller guarantees original writable GOT, same process, known ELF hash, and
 * no concurrent dynamic loader relocation. One atomic pointer CAS only. */
int au97_got_replace(Au97GotHook *hook,volatile uintptr_t *slot,
                     uintptr_t original,uintptr_t replacement);
/* Only for isolated host tests or after original owner has quiesced/unregistered
 * ALL callbacks and explicit drain. Never hot-unhook an OEM live callback. */
int au97_got_restore_quiesced(Au97GotHook *hook, int owner_drained);
/* Pins original MU1005 HfRfCallback GLOB_DAT relocation, only on QNX ARM32. */
int au97_install_original_hfp_observer(void);
#if defined(AU57X_R97_HOST_TEST)
int au97_test_install_hfp(volatile uintptr_t *slot, uintptr_t original);
#endif
void au97_hfp_observer_stats(Au97GotStats *out);
#endif
