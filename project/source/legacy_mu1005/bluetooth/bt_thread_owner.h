#ifndef AU57X_R97_BT_THREAD_OWNER_H
#define AU57X_R97_BT_THREAD_OWNER_H
/* R97: original btstack thread-origin observation, NOT a BlueSDK authorization.
 * No OEM calls, no synthetic pairing/MFi certificate, no extra worker thread.
 * The real pthread_create() implementation is provided by the interposer. */
#include <pthread.h>
#include <stdint.h>

typedef int (*Au97PthreadCreate)(pthread_t *, const pthread_attr_t *,
                                  void *(*)(void *), void *);
typedef struct {
    unsigned issued;
    unsigned running;
    unsigned finished;
    unsigned failed;
    unsigned fallback;
} Au97Stats;
/* Only these exact, original-MU1005 return addresses may be traced. */
int au97_is_stock_pthread_caller(uintptr_t return_address);
int au97_pthread_create(pthread_t *, const pthread_attr_t *,
                        void *(*)(void *), void *, uintptr_t return_address,
                        Au97PthreadCreate real_create);
/* True only INSIDE a currently running original-thread trampoline created
 * from a pinned OEM callsite. This DOES NOT prove RFCOMM event-thread ownership. */
int au97_current_observed_thread(void);
int au97_current_observed_site(uintptr_t return_address);
void au97_stats(Au97Stats *out);
#endif
