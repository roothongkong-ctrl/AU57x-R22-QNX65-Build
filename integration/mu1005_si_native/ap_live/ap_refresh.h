/* Review-only OEM AP snapshot to authenticated bootstrap bridge. */
#ifndef MU1005_AP_REFRESH_H
#define MU1005_AP_REFRESH_H
#include "ap_live.h"
#include "cp_cloud_pipeline.h"
/* Caller owns zero-initialized slots and serializes all access; never move
 * a slot or change a published slot while the bootstrap references it.
 * Run prepare outside SDK callbacks; commit on the original owner loop. */
struct mu_ap_refresh_slot {
 struct mu_ap_live ap;
 struct cp_session session;
 uint32_t epoch,generation;
 enum cp_radio_band band;
 int prepared;
};
typedef int (*mu_ap_verify_current)(void*,uint32_t,uint32_t,enum cp_radio_band,const struct mu_ap_live*);
int mu_ap_refresh_prepare(struct mu_ap_refresh_slot*,const struct cp_cloud_pipeline*,mu_ap_query,void*);
int mu_ap_refresh_commit(struct mu_ap_refresh_slot*,struct cp_cloud_pipeline*,mu_ap_verify_current,void*);
int mu_ap_refresh_clear(struct mu_ap_refresh_slot*,const struct cp_cloud_pipeline*);
/* Worker stage: snapshot live AP and select/install its exact 2.4 GHz
 * advertisement. Commit on serialized owner loop with independent current
 * AP/IE proof, as before. Caller retains lease even on failure. */
int mu_ap_refresh_prepare_first_link(struct mu_ap_refresh_slot*,const struct cp_cloud_pipeline*,struct mu_ap_ie_lease*,const struct mu_ap_ie_ops*,const uint8_t[6],const uint8_t[6]);
#endif
