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
};
struct cp_cloud_pipeline {
 struct cp_step04_session *bootstrap;
 struct cp_radio_policy radio;
 Mu34ApServer *airplay;
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
int cp_cloud_confirm_bootstrap_auth(struct cp_cloud_pipeline *);
int cp_cloud_radio_start(struct cp_cloud_pipeline *);
int cp_cloud_radio_result(struct cp_cloud_pipeline *,int association_succeeded,
                          int physical_disconnect_verified);
int cp_cloud_carplay_request(struct cp_cloud_pipeline *);
int cp_cloud_airplay_ready(struct cp_cloud_pipeline *,const char *observed_interface);
int cp_cloud_inner_auth(struct cp_cloud_pipeline *);
int cp_cloud_media_ready(struct cp_cloud_pipeline *);
int cp_cloud_bt_closed(struct cp_cloud_pipeline *,uint32_t generation);
int cp_cloud_wired_preempt(struct cp_cloud_pipeline *);
int cp_cloud_end(struct cp_cloud_pipeline *);
int cp_cloud_is_active(const struct cp_cloud_pipeline *);
#endif
