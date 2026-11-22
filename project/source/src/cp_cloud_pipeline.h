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
/* Advance R68 using binder-created native CFString, never guessed names. */
int cp_cloud_drive_native(struct cp_cloud_pipeline *,uint64_t now_ms);
/* Bind the *one* original-MFI iAP2 link, identity and current AP config while
 * the Bluetooth link is IDLE. The resulting proof callback reads real owner
 * and receiver state, not caller-provided synthetic readiness booleans. */
int cp_cloud_bind_bootstrap(struct cp_cloud_pipeline *,
  struct cp_step04_session *,const struct cp_session *,
  const struct cp_identification *);
int cp_cloud_confirm_bootstrap_auth(struct cp_cloud_pipeline *);
int cp_cloud_radio_start(struct cp_cloud_pipeline *);
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
int cp_cloud_wired_preempt(struct cp_cloud_pipeline *);
int cp_cloud_end(struct cp_cloud_pipeline *);
int cp_cloud_is_active(const struct cp_cloud_pipeline *);
#endif
