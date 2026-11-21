/* SPDX-License-Identifier: GPL-3.0-only
 * AU57x MU1005: AirPlay receiver + DIO guarded session handoff.
 * Source-level integration only. NO MFi, iPhone handshake, service injection,
 * device configuration, or proof that stock CarPlay supports wireless.
 */
#ifndef CP_R12_PIPELINE_H
#define CP_R12_PIPELINE_H
#include "cp_oem_handoff.h"
#include "native_exports.h"
#include <stdint.h>

enum cp_r12_result {
    CP_R12_OK=0, CP_R12_PENDING=1, CP_R12_LISTENING=2,
    CP_R12_INVALID=-1, CP_R12_OWNER=-2, CP_R12_ORDER=-3,
    CP_R12_NATIVE=-4, CP_R12_STALE=-5, CP_R12_WIRED=-6
};
/* Every callback is supplied by the *genuine* OEM receiver owner, on its
 * serialized thread. Returning 1 is an assertion backed by the runtime,
 * NOT inferred from command success, socket existence, or static symbols.
 * phone_tag identifies the SAME authenticated physical handset; not a MAC
 * guessed from a local network scan or a new synthetic identity.
 */
struct cp_r12_evidence {
    void *opaque;
    int (*wired_idle)(void *);
    int (*wifi_peer_live)(void *,uint32_t,const char *);
    int (*same_phone)(void *,uint32_t,uint64_t);
    int (*airplay_session_live)(void *,uint32_t,uint64_t);
    int (*inner_iap_authenticated)(void *,uint32_t,uint64_t);
    int (*dio_frames_and_controls)(void *,uint32_t,uint64_t);
};
struct cp_r12_pipeline {
    struct cp_oem_handoff *handoff; /* provided by actual authenticated BT owner */
    MuDioNative dio;
    struct cp_r12_evidence evidence;
    uint32_t epoch;
    uint64_t phone_tag;
    unsigned port;
    unsigned attached:1;
    unsigned start_attempted:1;
    unsigned listener_verified:1;
    unsigned session_verified:1;
    unsigned inner_verified:1;
    unsigned media_verified:1;
    unsigned stopping:1;
    unsigned wire_preempt_pending:1;
};
void cp_r12_init(struct cp_r12_pipeline *p);
/* Requires R11 OEM handoff verified all the way to WIFI_ASSOCIATED.
 * No guessed AirPlayServer pointer or CFString; binder verifies exports and
 * existing singleton against actual owner and keeps original interface.
 * The caller must hold sole ownership until cp_r12_stop() succeeds.
 */
int cp_r12_attach(struct cp_r12_pipeline *p,struct cp_oem_handoff *h,
                  const struct cp_r12_evidence *ev,void *loaded_libairplay,
                  void *existing_server,const void *existing_dio_manager,
                  const MuDioGuard *guard,const char *original_iface,
                  unsigned expected_port);
/* Must observe that real authenticated iAP2 sent CarPlay Start before calling.
 * An accepted startServer command means PENDING, not an AirPlay session.
 */
int cp_r12_start(struct cp_r12_pipeline *p);
int cp_r12_poll_listener(struct cp_r12_pipeline *p);
/* The second iAP2 session is INSIDE the actual AirPlay event/control channel.
 * Never upgrade first/bootstrap Bluetooth authentication to this state.
 */
int cp_r12_observe_airplay(struct cp_r12_pipeline *p,uint64_t phone_tag);
int cp_r12_observe_inner_iap2(struct cp_r12_pipeline *p,uint64_t phone_tag);
int cp_r12_observe_dio_media(struct cp_r12_pipeline *p,uint64_t phone_tag);
/* Recheck all independent OEM evidence on each media permission check. */
int cp_r12_media_ready(struct cp_r12_pipeline *p);
/* R18: once stop returns 0 after verified native restoration, this same
 * pipeline object can be attached to a fresh OEM epoch; cached identity and
 * start flags are reset. No attach may occur while restore is pending.
 * Stop is retryable: 1=pending, negative=cleanup blocked. Never free/move
 * this object until 0 AND the external OEM has confirmed teardown. */
int cp_r12_stop(struct cp_r12_pipeline *p);
/* Wired takeover latches immediately, native restore runs while its owner
 * lease is still alive. For PENDING/ERROR the owner objects must remain
 * pinned; retry cp_r12_stop() until restoration is confirmed, at which point
 * the borrowed OEM handoff is revoked. Not thread-safe; OEM owner-thread only. */
int cp_r12_wired_preempt(struct cp_r12_pipeline *p,int owner_confirmed);
#endif
