/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AU57X_R100_ROUTE_GATE_H
#define AU57X_R100_ROUTE_GATE_H
/* R100: decide which already-verified runtime bridge MAY be attempted.
 * The decision is never an MFi or iPhone authentication claim.
 */
typedef enum {
    AU100_NO_GO = 0,
    AU100_FACTORY_IAP2_ROUTE = 1,
    AU100_GRAFT_RFCOMM_ROUTE = 2
} Au100Route;

typedef struct {
    unsigned stock_image_pinned;
    unsigned original_config_backed_up;
    unsigned watchdog_and_recovery_installed;
    unsigned wired_carplay_untouched;
    /* Factory path: every field is a runtime observation, not just a binary string. */
    unsigned factory_iap2_config_enabled;
    unsigned factory_endpoint_live;
    unsigned factory_rfcomm_bound;
    /* Graft path: injection is explicit and only one iAP2 owner may exist. */
    unsigned user_authorized_graft;
    unsigned exact_btstack_process_proven;
    unsigned stock_blue_sdk_thread_proven;
    unsigned rf_register_profile_proven;
    unsigned native_callback_owner_proven;
    unsigned factory_iap2_quiesced;
    unsigned original_rf_rollback_barrier_proven;
} Au100Evidence;

Au100Route au100_route_select(const Au100Evidence *e);
const char *au100_route_name(Au100Route r);
#endif
