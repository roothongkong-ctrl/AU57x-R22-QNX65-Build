/* SPDX-License-Identifier: GPL-3.0-only
 * AU57x MU1005: owner-attested coordination of existing iAP2/MFi, radio,
 * AirPlay, SI/DIO and media contracts. This code is review-only; no OEM hooks.
 */
#ifndef CP_CLOUD_PIPELINE_H
#define CP_CLOUD_PIPELINE_H
#include "cp_step04_session.h"
#include "cp_radio_policy.h"
#include "cp_session_handoff.h"
#include "mu1005_r34_ap_server.h"
#include "native_exports.h"
#include "cprx_service.h"
#include <stddef.h>
#include <stdint.h>
#define CP_CLOUD_IFACE_MAX 16u
#define CP_CLOUD_PORT 5000u

enum cp_cloud_result {
 CP_CLOUD_OK=0, CP_CLOUD_RETRY=1, CP_CLOUD_BAD=-1,
 CP_CLOUD_ORDER=-2, CP_CLOUD_PROOF=-3, CP_CLOUD_STALE=-4,
 CP_CLOUD_STOPPED=-5
};
enum cp_cloud_media { CP_CLOUD_VIDEO=1,CP_CLOUD_AUDIO=2,
                      CP_CLOUD_HMI=3,CP_CLOUD_STEERING=4 };
struct cp_cloud_owner {
 void *ctx;
 /* Every callback needs an independently verified, current OEM owner,
  * not an assertion copied from a message received over Bluetooth. */
 int (*bt_alive)(void *,uint32_t epoch,uint32_t bt_generation);
 int (*wired_idle)(void *);
 int (*radio_capability)(void *,enum cp_radio_band);
 int (*ap_prepare)(void *,uint32_t epoch,enum cp_radio_band);
 int (*ap_association)(void *,uint32_t epoch,enum cp_radio_band);
 int (*ap_disconnected)(void *,uint32_t epoch,enum cp_radio_band);
 int (*carplay_request_sent)(void *,uint32_t epoch);
 int (*si_single_owner)(void *,uint32_t epoch);
 int (*bonjour_identity_verified)(void *,uint32_t epoch,const char *iface);
 int (*airplay_listener_verified)(void *,uint32_t epoch,const char *iface,unsigned port);
 int (*inner_iap2_mfi_verified)(void *,uint32_t epoch);
 int (*media_verified)(void *,uint32_t epoch,enum cp_cloud_media item);
 int (*teardown_confirmed)(void *,uint32_t epoch);
 /* Actual running AP beacon/channel, before the iPhone associates.
  * Merely accepting ap_prepare() is not sufficient evidence. */
 int (*ap_hotspot_verified)(void *,uint32_t epoch,enum cp_radio_band);
 /* Actual connected iPhone AirPlay/CarPlay session; not merely Bonjour
  * advertisement or an open listening socket on port 5000. */
 int (*airplay_session_connected)(void *,uint32_t epoch,const char *iface,unsigned port);
};
struct cp_cloud_pipeline {
 struct cp_step04_session *bootstrap;
 struct cp_radio_policy radio;
 Mu34ApServer *airplay;
 /* When present, genuine same-process DIO backend owns this receiver. */
 MuDioNative *native_dio;
 /* SI-owned receiver service witnesses the same inner iAP2 and media session. */
 MuCprxService *rx_service;
 /* Explicit opt-in: the original SI owner may drive a real nonblocking
  * AF_UNIX START client. Never auto-start an unverified receiver. */
 MuCprxClient rx_client;
 int rx_client_enabled;
 /* R79: same sender drives media-aware STOP and awaits OEM restore ACK. */
 int graceful_stop_started;
 /* A failed native lease owns a mandatory, retryable OEM restore barrier. */
 int native_cleanup_pending;
 /* R84: SI and DIO are separate ORIGINAL processes. Never put DIO's
  * MuCprxService * or MuDioNative * in an SI pipeline on the car. */
 MuCprxRemoteEndpoint remote_dio;
 MuCprxClient remote_client;
 unsigned remote_phase; /* 0 unbound, 1 START, 2 snapshot, 3 STOP, 4 restored */
 int remote_start_confirmed,remote_snapshot_fresh;
 /* Historical bootstrap evidence, never a substitute for live inner MFi. */
 int remote_bootstrap_committed;
 int remote_media,remote_inner;
 unsigned remote_channels;
 uint64_t remote_sample_ms;
 int remote_wired_pending; /* op12 pending until independent DIO verified */
 int remote_teardown_confirmed; /* DIO restore ACK != full SI teardown */
 struct cp_handoff handoff;
 struct cp_cloud_owner owner;
 uint32_t epoch,bt_generation;
 enum cp_radio_band band;
 char iface[CP_CLOUD_IFACE_MAX+1];
 int started,stopped;
};
void cp_cloud_init(struct cp_cloud_pipeline *);
int cp_cloud_begin(struct cp_cloud_pipeline *,struct cp_step04_session *,
                   const struct cp_cloud_owner *,uint32_t epoch,uint32_t bt_generation);
/* Requires exact known OEM AirPlay backend; never a different-firmware donor. */
int cp_cloud_bind_airplay(struct cp_cloud_pipeline *,Mu34ApServer *);
/* Bind original MU1005 AirPlay singleton + SI owner, verified by
 * mu_dio_bind_interface() INSIDE the original OEM process and thread. */
int cp_cloud_bind_native_dio(struct cp_cloud_pipeline *,MuDioNative *);
/* Run SI CPRX tick, iAP2 inner-auth and native media confirmation in one
 * owner-thread loop. Return OK only when MEDIA_ACTIVE remains live. */
int cp_cloud_drive_native(struct cp_cloud_pipeline *,uint64_t now_ms);
/* Bind an SI-owned v3 CPRX lease BEFORE its first START, avoiding competing
 * native AirPlay start operations. The local SI CPRX socket is ticked through
 * cp_cloud_drive_native on the original owner thread. */
int cp_cloud_bind_rx_service(struct cp_cloud_pipeline *,MuCprxService *);
/* Owner-only opt-in after binding the authentic CPRX receiver. Once enabled,
 * drive_native sends the real v3 START and waits for an actual socket ACK. */
int cp_cloud_enable_native_client(struct cp_cloud_pipeline *);
/* R84: SI-only cross-process pipeline. The original DIO receiver lives in
 * another process. Bind via the pinned AF_UNIX server endpoint; do not
 * dereference/copy a DIO process pointer or synthesize inner MFi readiness.
 * The caller supplies the observed OEM Wi-Fi interface. */
int cp_cloud_bind_remote_dio(struct cp_cloud_pipeline *,
 const MuCprxRemoteEndpoint *,const char *observed_iface);
/* SI original-owner tick; a separate DIO original-owner tick services its
 * socket. Reports OK only on a FRESH same-session op11 snapshot + original
 * owner iAP2/4301/media evidence, never on an old START ACK. */
int cp_cloud_drive_remote_dio(struct cp_cloud_pipeline *,uint64_t now_ms);
/* Caller supplies a monotonic time; this is a time-bounded observation,
 * not permanently granted streaming media. */
int cp_cloud_remote_live_at(const struct cp_cloud_pipeline *,uint64_t now_ms);
/* First-video observation independent of optional audio/control channels.
 * Still requires the actual same-session OEM video and inner MFi witnesses.
 * Not an assertion of LCD output or full CarPlay feature completion. */
int cp_cloud_remote_video_at(const struct cp_cloud_pipeline *,uint64_t now_ms);
/* Normal idle STOP only; preserves R79 streaming BUSY semantics and waits
 * for DIO's actual original restore ACK. For wired takeover, the owner first
 * calls cp_cloud_wired_preempt(), then repeatedly calls drive_remote_dio()
 * to execute R85 opt-in op12. Without independent SI/DIO original physical
 * wired proofs, the restore remains RETRY and is never reported complete. */
int cp_cloud_remote_graceful_end(struct cp_cloud_pipeline *,uint64_t now_ms);
/* Normal user disconnect: nonblocking SI v3 STOP on the same OEM owner
 * thread; returns RETRY for streaming/BUSY/in-progress OEM restoration and
 * OK only after the authenticated STOP ACK and all teardown proofs.
 * Wired preemption must still use cp_cloud_wired_preempt(). */
int cp_cloud_graceful_native_end(struct cp_cloud_pipeline *,uint64_t now_ms);
/* Require the real current SI lease (generation+epoch+session), with
 * independently observed native owner proof; optionally native media=7.
 * R74 binds inner iAP2/MFi and four channels to this exact SI session. */
int cp_cloud_native_rx_live(const struct cp_cloud_pipeline *,int require_media);
/* Bind the *one* original-MFI iAP2 link, identity and current AP config while
 * the Bluetooth link is IDLE. The resulting proof callback reads real owner
 * and receiver state, not caller-provided synthetic readiness booleans. */
int cp_cloud_bind_bootstrap(struct cp_cloud_pipeline *,
  struct cp_step04_session *,const struct cp_session *,
  const struct cp_identification *);
int cp_cloud_confirm_bootstrap_auth(struct cp_cloud_pipeline *);
int cp_cloud_radio_start(struct cp_cloud_pipeline *);
/* Rebind a caller-owned immutable AP snapshot after a verified band
 * change, before association/start. No credential generation or radio I/O. */
int cp_cloud_refresh_ap_config(struct cp_cloud_pipeline *,const struct cp_session *,
 int (*verify)(void *,uint32_t,enum cp_radio_band,const struct cp_session *),void *);
int cp_cloud_radio_result(struct cp_cloud_pipeline *,int association_succeeded,
                          int physical_disconnect_verified);
/* Drive the ACTUAL already-bound OEM AirPlay server on the original owner
 * thread, using the original CFString for the observed Wi-Fi interface.
 * Returns RETRY until port 5000, Bonjour and SI identity are verified. */
int cp_cloud_prepare_receiver(struct cp_cloud_pipeline *,const void *cf_interface);
/* Confirms the authenticated 0x4301 reached link ACK, not merely an
 * externally asserted 'request sent' flag. */
int cp_cloud_carplay_request(struct cp_cloud_pipeline *);
/* Call periodically on original OEM owner thread during WIFI_ASSOCIATED.
 * Starts the genuine AirPlay receiver once, pumps the existing iAP2 link,
 * waits for 4301 peer ACK, then advances to native AirPlay session state.
 * Never creates a background thread or a synthetic transport. */
int cp_cloud_drive_start(struct cp_cloud_pipeline *,const void *cf_interface,
                         uint64_t now_ms);
int cp_cloud_airplay_ready(struct cp_cloud_pipeline *,const char *observed_interface);
int cp_cloud_inner_auth(struct cp_cloud_pipeline *);
int cp_cloud_media_ready(struct cp_cloud_pipeline *);
int cp_cloud_bt_closed(struct cp_cloud_pipeline *,uint32_t generation);
/* Always retry pending OEM restoration even after the initial wired takeover. */
int cp_cloud_wired_preempt(struct cp_cloud_pipeline *);
int cp_cloud_end(struct cp_cloud_pipeline *);
int cp_cloud_is_active(const struct cp_cloud_pipeline *);
#endif
