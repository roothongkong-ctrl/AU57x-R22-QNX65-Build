/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CP_WCP_R45_CLOUD_H
#define CP_WCP_R45_CLOUD_H
#include "cp_wcp_r45_remote.h"
#include "cp_wcp_handoff.h"
/* One serialized coordinator for the existing R45 transport and original
 * receiver route. Before first tick the caller binds original identity,
 * AP config, stock MFi and verified socket using the existing APIs.
 * AP worker publishes a fresh verified config via cp_cloud_refresh_ap_config;
 * the independent DIO process supplies its actual admitted endpoint.
 * All objects/owner callbacks live until cleanup. Do not run another socket
 * poll or first-connect loop concurrently. No new pairing or receiver. */
int cp_wcp_r45_cloud_tick(struct cp_wcp_r45_remote*,struct cp_s4_socket_io*,
 struct cp_s4_tcp_bridge*,const struct cp_wcp_r45_remote_owner*,
 struct cp_wcp_autoconnect*,struct cp_cloud_pipeline*,const struct cp_cloud_owner*,
 const MuCprxRemoteEndpoint*,const char*,uint64_t);
#endif
