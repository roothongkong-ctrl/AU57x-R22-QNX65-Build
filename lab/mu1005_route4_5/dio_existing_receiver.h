/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_DIO_EXISTING_RECEIVER_H
#define MU1005_DIO_EXISTING_RECEIVER_H
#include "cprx_service.h"
/* Call from the original DIO owner after its receiver is created and idle.
 * No dlopen, no new receiver, no replacement delegate. handle is the already
 * loaded, verified stock libairplay. Manager and exclusive lifetime proof
 * must come from actual DIO, not from the global receiver pointer.
 * 0 control socket opened (not an active AirPlay session), -1 rejected,
 * -2 binding retained for explicit owner restore. Caller retains storage. */
int mu_dio_existing_receiver_open(MuCprxService*,MuDioNative*,void*,const void*,
 uint32_t,const MuDioGuard*,const char*,const char*,const MuCprxEvidence*,
 unsigned,uint64_t,uint32_t,const char*,uid_t,gid_t);
#endif
