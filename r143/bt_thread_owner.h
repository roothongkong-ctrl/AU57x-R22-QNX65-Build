#ifndef AU57X_R143_BT_THREAD_OWNER_H
#define AU57X_R143_BT_THREAD_OWNER_H
#include <pthread.h>
#include <stdint.h>
typedef int (*Au97PthreadCreate)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
typedef struct { unsigned issued, running, finished, failed, fallback; } Au97Stats;
int au97_is_stock_pthread_caller(uintptr_t return_address);
int au97_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *, uintptr_t return_address, Au97PthreadCreate real_create);
int au97_current_observed_thread(void);
int au97_current_observed_site(uintptr_t return_address);
int au97_is_observed_thread(pthread_t thread);
void au97_stats(Au97Stats *out);
#endif
