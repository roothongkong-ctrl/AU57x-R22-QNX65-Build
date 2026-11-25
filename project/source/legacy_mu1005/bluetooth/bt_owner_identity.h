#ifndef MU1005_BT_OWNER_IDENTITY_H
#define MU1005_BT_OWNER_IDENTITY_H
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
/* Read-only original OSAPI ActiveObject identity. The supplied libcomm
 * load bias must be independently verified against the pinned original.
 * A match proves thread identity only, not callback exclusion or SDK locks. */
typedef struct {
 uint32_t comm_bias, tid, factory, shared_control, active_object;
} MuBtThreadIdentity;
/* A checked reader must cover executable words as well as live objects.
 * Anchors are extra guards, not full mapped-image authentication. */
int mu_bt_comm_words_match(uint32_t,int(*)(void*,uint32_t,void*,size_t),void*);
int mu_bt_osapi_identity(uint32_t,uint32_t,int(*)(void*,uint32_t,void*,size_t),void*,MuBtThreadIdentity*);
/* Discover the queue's own thread, never claim the calling thread owns it.
 * Use only under an independent original lifecycle lease. */
int mu_bt_osapi_target(uint32_t,int(*)(void*,uint32_t,void*,size_t),void*,MuBtThreadIdentity*);
int mu_bt_osapi_identity_matches(const MuBtThreadIdentity*,uint32_t,int(*)(void*,uint32_t,void*,size_t),void*);
int mu_bt_osapi_thread_matches(uint32_t comm_bias,uint32_t caller_tid,
 int (*read_live)(void*,uint32_t,void*,size_t),void*);
#endif
