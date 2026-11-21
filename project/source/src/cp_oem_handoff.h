/* SPDX-License-Identifier: GPL-3.0-only
 * R10: OFFLINE OEM-boundary handoff adapter.  NOT a production BlueSDK hook.
 * Every attestation MUST be implemented by an independently verified,
 * authorized owner of the actual OEM callback and event thread.
 */
#ifndef CP_OEM_HANDOFF_H
#define CP_OEM_HANDOFF_H
#include "cp_step2_transport.h"
#include "cp_step2_ipc_link.h"
#include "cp_session_handoff.h"
#include <stddef.h>
#include <stdint.h>

enum cp_oem_gate_event {
 CP_OEM_BT_IAP_AUTH = 1,
 CP_OEM_WIFI_ASSOCIATED = 2,
 CP_OEM_CARPLAY_START_SENT = 3,
 CP_OEM_AIRPLAY_SESSION = 4,
 CP_OEM_INNER_IAP_AUTH = 5,
 CP_OEM_DIO_MEDIA = 6
};
/* All callbacks must read real OEM state; they cannot infer state from a
 * submitted command, a static firmware flag or a previous fake/test event.
 * Invoked only on the authorized external owner thread and never reentrantly.
 */
struct cp_oem_witness {
 void *context;
 int (*wired_idle)(void *);
 int (*bt_owner)(void *, uint32_t connection_generation);
    int (*verified_event)(void *, enum cp_oem_gate_event event,
                       uint32_t epoch, const char *interface_name);
    /* Needed when BlueSDK physical generation differs from cp_bt_bridge generation.
     * This MUST be independently observed by the genuine authorized owner,
     * not assumed because the two processes received OPEN events. */
    int (*bridge_mapping)(void *, uint32_t radio_generation,
                          uint32_t local_bridge_generation);
};
struct cp_oem_handoff {
 struct cp_handoff session;
 struct cp_step2_transport transport;
 struct cp_bt_bridge *bridge;
 /* Owner is unique: borrowed R11 transport OR the legacy local one. */
 struct cp_step2_transport *live_transport;
 struct cp_step2_ipc_link *borrowed_link;
 struct cp_oem_witness witness;
 uint32_t epoch, physical_generation, bridge_generation;
 char interface_name[17];
 int bound;
};
void cp_oem_handoff_init(struct cp_oem_handoff *h);
/* Strict two-generation entrypoint for R11/R7 IPC:
 * radio_generation is from real BlueSDK; bridge_generation is from cp_bt_bridge.
 * When different, caller MUST supply independently verified bridge_mapping.
 * transport_owner.verify/submit_async receive LOCAL generation only. */
int cp_oem_handoff_begin_mapped(struct cp_oem_handoff *h,
                         struct cp_bt_bridge *bridge,
                         const struct cp_step2_owner *transport_owner,
                         const struct cp_oem_witness *witness,
                         uint32_t physical_generation,
                         uint32_t local_bridge_generation, uint32_t epoch);
/* The R11 IPC link already owns cp_step2_transport. This variant borrows it,
 * never binds a second reader and requires verified physical-to-local mapping.
 * The same authorized owner must serialize R11 IPC feed and R12 notifications.
 * No provider of real BlueSDK callbacks is included in this offline project.
 */
int cp_oem_handoff_begin_from_ipc(struct cp_oem_handoff *h,
       struct cp_step2_ipc_link *existing_link,
       const struct cp_oem_witness *witness, uint32_t epoch);
/* bridge must already represent a real, authorized connection, and its
 * physical generation must come from the true SDK callback boundary.
 * Legacy API only supports the case physical==local; for R11 real IPC
 * use cp_oem_handoff_begin_mapped().
 * Does NOT establish, authorize, or start Bluetooth itself.
 */
int cp_oem_handoff_begin(struct cp_oem_handoff *h,
                         struct cp_bt_bridge *bridge,
                         const struct cp_step2_owner *transport_owner,
                         const struct cp_oem_witness *witness,
                         uint32_t physical_generation, uint32_t epoch);
/* No event is promoted without an independently attested OEM transition.
 * interface_name: exact observed interface (e.g. uap0), not a radio guess.
 */
int cp_oem_handoff_confirm(struct cp_oem_handoff *h,
                           enum cp_oem_gate_event ev,
                           const char *interface_name);
/* The external owner must have already confirmed the actual link closure.
 * After inner iAP2 authentication, Bluetooth can drop without killing media.
 */
int cp_oem_handoff_bt_closed(struct cp_oem_handoff *h,
                             uint32_t event_generation);
/* Wired preemption is independent of the wireless epoch.  Calling this does
 * not stop the real car's wireless receiver; the owner must separately do so.
 */
int cp_oem_handoff_wired(struct cp_oem_handoff *h, int active,
                         int owner_confirmed);
int cp_oem_handoff_teardown_confirmed(struct cp_oem_handoff *h,
                                      int owner_confirmed);
int cp_oem_handoff_media_ready(const struct cp_oem_handoff *h);
#endif
