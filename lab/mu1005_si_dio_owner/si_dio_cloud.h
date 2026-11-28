/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_SI_DIO_CLOUD_H
#define MU1005_SI_DIO_CLOUD_H
#include "si_dio_owner.h"
#include "cp_wcp_r45_cloud.h"
/* Original serialized SI supervisor owns this binding and all borrowed
 * storage through retirement. This is a review integration, not an OEM
 * owner-discovery mechanism. Endpoint must be certified for the exact child. */
typedef struct {
 MuSiDioOwner child;
 MuSiDioOps original;
 char *const *env;
 const char *library;
 int (*endpoint_owned)(void*,pid_t,uint32_t,const MuCprxRemoteEndpoint*);
 int launched;
} MuSiDioCloud;
/* Only after the existing authenticated cloud pipeline has independently
 * confirmed association. 1 = child spawned or pending endpoint, NOT ready. */
int mu_si_dio_after_wifi(MuSiDioCloud*,struct cp_cloud_pipeline*);
/* Run instead of (never alongside) cp_wcp_r45_cloud_tick on the original
 * SI owner. Endpoint supplied by existing DIO IPC owner after actual spawn.
 * No new pairing, no guessed environment, no forced process termination. */
int mu_si_dio_cloud_tick(MuSiDioCloud*,struct cp_wcp_r45_remote*,struct cp_s4_socket_io*,
 struct cp_s4_tcp_bridge*,const struct cp_wcp_r45_remote_owner*,struct cp_wcp_autoconnect*,
 struct cp_cloud_pipeline*,const struct cp_cloud_owner*,const MuCprxRemoteEndpoint*,
 const char*,uint64_t);
#endif
