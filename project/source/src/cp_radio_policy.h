/* SPDX-License-Identifier: GPL-3.0-only
 * Deterministic and strictly attested wireless radio band policy.
 * Not a Wi-Fi driver: caller must obtain authoritative MU1005 radio/AP evidence.
 */
#ifndef CP_RADIO_POLICY_H
#define CP_RADIO_POLICY_H
#include <stdint.h>
enum cp_radio_band { CP_RADIO_NONE=0, CP_RADIO_24=24, CP_RADIO_5=50 };
enum cp_radio_result {
 CP_RADIO_OK=0, CP_RADIO_PENDING=1, CP_RADIO_NO_FALLBACK=2,
 CP_RADIO_INVALID=-1, CP_RADIO_STALE=-2, CP_RADIO_NO_OWNER=-3,
 CP_RADIO_BAD_STATE=-4, CP_RADIO_BLOCKED=-5
};
enum cp_radio_phase { CP_RADIO_IDLE=0, CP_RADIO_CHOSEN=1,
 CP_RADIO_CONNECTING=2, CP_RADIO_ASSOCIATED=3, CP_RADIO_CLOSED=4 };
struct cp_radio_policy {
 uint32_t epoch, last_epoch;
 unsigned cap24:1, cap5:1;
 unsigned tried24:1, tried5:1;
 unsigned wired:1, owner_lost:1;
 enum cp_radio_phase phase;
 enum cp_radio_band selected,preferred;
};
/* cap24/cap5 MUST be positively proven on actual radio, never inferred from
 * configured SSID, iAP2 Wi-Fi strings, or marketing claims.
 * One controller per epoch; neither function controls actual hardware. */
void cp_radio_init(struct cp_radio_policy *p);
int cp_radio_begin(struct cp_radio_policy *p, uint32_t epoch,
                   int cap24_confirmed, int cap5_confirmed,
                   int owner_confirmed, int wired_confirmed_idle);
/* Explicit first-connect preference. Legacy begin defaults to 5 GHz. */
int cp_radio_begin_preferred(struct cp_radio_policy *p,uint32_t epoch,
 int cap24,int cap5,int owner,int wired_idle,enum cp_radio_band preferred);
/* Classify configured 2.4/5 GHz channel; not a regulatory/channel capability check. */
enum cp_radio_band cp_radio_channel_band(unsigned channel);
int cp_radio_select(struct cp_radio_policy *p,uint32_t epoch,
                    int owner_confirmed,enum cp_radio_band *band);
int cp_radio_connect_requested(struct cp_radio_policy *p,uint32_t epoch,
                               int owner_confirmed);
int cp_radio_association_result(struct cp_radio_policy *p,uint32_t epoch,
                                int owner_confirmed,int associated,
                                enum cp_radio_band observed_band,
                                int disconnect_confirmed);
int cp_radio_revoke(struct cp_radio_policy *p,uint32_t epoch,
                    int wired_started);
int cp_radio_new_epoch_ready(struct cp_radio_policy *p,uint32_t epoch,
                             int actual_teardown_confirmed);
#endif
