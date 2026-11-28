/* SPDX-License-Identifier: GPL-3.0-only
 * AU57x / MU1005: owner-attested car-initiated stock iAP2 RFCOMM request.
 * Independently reimplemented from publicly documented WCP lifecycle.
 * No donor binary, static vendor address or MFi bypass is included.
 * R87 SOURCE ONLY. A real, pinned OEM SDK adapter is REQUIRED on the car.
 */
#ifndef CP_WCP_AUTOCONNECT_H
#define CP_WCP_AUTOCONNECT_H
#include <stdint.h>

#define CP_WCP_INITIAL_MS UINT64_C(4000)
#define CP_WCP_OPEN_TIMEOUT_MS UINT64_C(12000)
#define CP_WCP_RETRY_MS UINT64_C(5000)
#define CP_WCP_NO_SERVICE_MS UINT64_C(30000)
#define CP_WCP_MAX_ATTEMPTS 3u

enum cp_wcp_state {
 CP_WCP_IDLE=0,CP_WCP_DELAY,CP_WCP_OPENING,
 CP_WCP_BACKOFF,CP_WCP_READY,CP_WCP_RESTORE_PENDING,
 CP_WCP_BLOCKED
};
enum cp_wcp_rc {
 CP_WCP_OK=0,CP_WCP_WAIT=1,CP_WCP_LINK_READY=2,
 CP_WCP_DENIED=-1,CP_WCP_STALE=-2,CP_WCP_CLOCK=-3,
 CP_WCP_INVALID=-4
};
/* Normalized result for stock_open_iap2, not native SDK return values. */
enum cp_wcp_open_result {
 CP_WCP_OPEN_REJECTED=-1,CP_WCP_OPEN_UNCERTAIN=-2,
 CP_WCP_OPEN_BUSY=0,CP_WCP_OPEN_PENDING=1
};
/* R105.239: all five original RFCOMM callers compare result against 2.
 * Non-2 is NOT yet classified for wireless. A real owner may assert
 * no_request_proven only with independent original release evidence. */
int cp_wcp_stock_client_result(int raw,int no_request_proven);
struct cp_wcp_bt_ops {
 void *ctx;
 /* These are exact current OEM generation/owner observations, not BT
  * advertisements, fixed binary addresses or caller-provided "is ready". */
 int (*stock_owner_verified)(void *,uint32_t generation);
 int (*paired_acl_verified)(void *,uint32_t generation);
 int (*wired_idle_verified)(void *);
 int (*stock_iap2_connected)(void *,uint32_t generation);
 /* 1 = verified phone-side SDP iAP2, 0 = confirmed absent, -1 = unknown. */
 int (*phone_iap2_sdp)(void *,uint32_t generation);
 /* Enqueue an original BlueSDK client-open on its owner thread;
  * CP_WCP_OPEN_PENDING=request accepted, not link ready; BUSY/REJECTED
  * only before any native request is outstanding. UNCERTAIN requires
  * original cancel/teardown acknowledgement. No Bluetooth shortcuts. */
 int (*stock_open_iap2)(void *,uint32_t generation);
 /* Cancel a previously requested operation. 1 = OEM confirms released. */
 int (*stock_cancel_iap2)(void *,uint32_t generation);
};
struct cp_wcp_autoconnect {
 struct cp_wcp_bt_ops ops;
 enum cp_wcp_state state;
 uint32_t generation;
 uint64_t last_ms,deadline_ms;
 unsigned attempts;
 int requested;
};
int cp_wcp_bt_init(struct cp_wcp_autoconnect *,const struct cp_wcp_bt_ops *);
/* Only call with real paired ACL evidence from original btstack. */
int cp_wcp_bt_acl_up(struct cp_wcp_autoconnect *,uint32_t generation,uint64_t now_ms);
/* Owner-loop recovery after confirmed prior teardown and a newer verified
 * paired ACL. Does not reset the controller, accept old events, bypass
 * wired priority, or permit reuse of a still-pending native request.
 * Generation is monotonic for this controller lifetime; on exhaustion,
 * only a fully torn-down owner may construct a fresh controller. */
int cp_wcp_bt_rearm(struct cp_wcp_autoconnect *,uint32_t generation,uint64_t now_ms,
 int (*old_release_verified)(void *,uint32_t previous_generation));
/* Call from the original serialized owner loop; never inside SDK callbacks. */
int cp_wcp_bt_tick(struct cp_wcp_autoconnect *,uint32_t generation,uint64_t now_ms);
/* Disconnect or wired priority requires OEM cancel acknowledgement first. */
int cp_wcp_bt_abort(struct cp_wcp_autoconnect *,uint32_t generation,int wired);
/* If previous restore was blocked, the OEM owner can retry cancellation. */
int cp_wcp_bt_restore_tick(struct cp_wcp_autoconnect *,uint32_t generation);
#endif
