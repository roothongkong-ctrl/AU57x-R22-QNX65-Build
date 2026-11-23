/* SPDX-License-Identifier: GPL-3.0-only
 * Safe factory -> graft fallback eligibility. This module does not install,
 * start, or patch any vehicle service. Runtime proofs must be independent.
 */
#include "bt_route_gate.h"
Au100Route au100_route_select(const Au100Evidence *e) {
    if (!e || e->stock_image_pinned != 1u ||
        e->original_config_backed_up != 1u ||
        e->watchdog_and_recovery_installed != 1u ||
        e->wired_carplay_untouched != 1u)
        return AU100_NO_GO;
    /* Never use 'BluetoothiAP2Link' in a binary as endpoint proof. */
    if (e->factory_iap2_config_enabled == 1u &&
        e->factory_endpoint_live == 1u &&
        e->factory_rfcomm_bound == 1u)
        return AU100_FACTORY_IAP2_ROUTE;
    /* A profile-0 guess or HFP worker is not an owner proof. */
    if (e->user_authorized_graft == 1u &&
        e->exact_btstack_process_proven == 1u &&
        e->stock_blue_sdk_thread_proven == 1u &&
        e->rf_register_profile_proven == 1u &&
        e->native_callback_owner_proven == 1u &&
        e->factory_iap2_quiesced == 1u &&
        e->original_rf_rollback_barrier_proven == 1u)
        return AU100_GRAFT_RFCOMM_ROUTE;
    return AU100_NO_GO;
}
const char *au100_route_name(Au100Route route) {
    switch(route) {
        case AU100_FACTORY_IAP2_ROUTE: return "FACTORY_IAP2_RUNTIME_VERIFIED";
        case AU100_GRAFT_RFCOMM_ROUTE: return "GRAFT_RUNTIME_ELIGIBLE";
        default: return "NO_GO";
    }
}
