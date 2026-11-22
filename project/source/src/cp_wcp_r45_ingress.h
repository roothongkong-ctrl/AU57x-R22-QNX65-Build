/* SPDX-License-Identifier: GPL-3.0-only
 * R88 MU1005 stock RFCOMM SERVER-FIRST path. QNX-safe C99, NO INSTALL.
 * No assumed /dev/iapDevice pathname, no forged SDP, no OEM code execution.
 */
#ifndef CP_WCP_R45_INGRESS_H
#define CP_WCP_R45_INGRESS_H
#include <stdint.h>
#include "cp_wcp_autoconnect.h"
#include "mu1005_rf_ingress.h"
/* These callbacks MUST be supplied by an independently attested original
 * btstack callback owner, not from a file marker or a phone advertisement.
 * The production callback attachment has NOT yet been proven on MU1005. */
struct cp_wcp_r45_owner {
 void *ctx;
 int (*owner_verified)(void *,uint32_t);
 int (*wired_idle)(void *);
 int (*original_rf_server_published)(void *,uint32_t);
 int (*paired_acl_authorized)(void *,uint32_t,const uint8_t peer[6]);
 int (*r45_transport_live)(void *,uint32_t,uint32_t rf_generation);
};
struct cp_wcp_r45_ingress {
 const AuRfIngress *rf; /* borrowed, owned by the verified R45 native loop */
 struct cp_wcp_r45_owner o;
 uint32_t generation,rf_generation;
 uint8_t peer[6];
 int bound;
};
/* Call only AFTER a genuine, paired, stock RFCOMM event 2 reached R45 and
 * au_rf_connected() succeeded.  Never bind to a name-only discovery event. */
int cp_wcp_r45_bind(struct cp_wcp_r45_ingress *,const AuRfIngress *,
                   const struct cp_wcp_r45_owner *,uint32_t generation);
/* Returns R87 callbacks for an already connected R45 *server* RFCOMM link.
 * Native client SDP/open/cancel methods are intentionally not implemented. */
int cp_wcp_r45_ops(struct cp_wcp_r45_ingress *,struct cp_wcp_bt_ops *);
void cp_wcp_r45_unbind(struct cp_wcp_r45_ingress *);
#endif
