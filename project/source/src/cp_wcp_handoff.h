/* SPDX-License-Identifier: GPL-3.0-only
 * R87: connect independently attested car-initiated BT link to R44 OEM MFi
 * and original R85 cloud session controller. Never authorize via BT alone.
 * SOURCE ONLY / NO INSTALL.
 */
#ifndef CP_WCP_HANDOFF_H
#define CP_WCP_HANDOFF_H
#include "cp_wcp_autoconnect.h"
#include "cp_cloud_pipeline.h"
/* Must be called from SI's original serialized owner loop. A completed
 * Bluetooth link alone is NOT an MFi/CarPlay session.  A properly received
 * iAP2 CONTROL AA05 and OEM peer attestation must already exist in R44. */
int cp_wcp_begin_cloud(struct cp_wcp_autoconnect *,
 struct cp_step04_session *,struct cp_cloud_pipeline *,
 const struct cp_cloud_owner *);
#endif
