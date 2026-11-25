#ifndef MU1005_BT_OWNER_IDENTITY_H
#define MU1005_BT_OWNER_IDENTITY_H
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
/* Read-only original OSAPI ActiveObject identity. The supplied libcomm
 * load bias must be independently verified against the pinned original.
 * A match proves thread identity only, not callback exclusion or SDK locks. */
int mu_bt_osapi_thread_matches(uint32_t comm_bias,uint32_t caller_tid,
 int (*read_live)(void*,uint32_t,void*,size_t),void*);
#endif
