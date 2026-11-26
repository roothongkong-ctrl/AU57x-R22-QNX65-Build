#include "mu1005_owner_gate.h"
#include <string.h>
#define EXPECTED_BTSTACK_SHA256 "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
int au57x_x3_owner_gate(const Au57xX3Evidence *e) {
    if (!e || !e->btstack_sha256) return AU57X_X3_DENY;
    if (strcmp(e->btstack_sha256, EXPECTED_BTSTACK_SHA256) != 0) return AU57X_X3_DENY;
    if (!e->owner_generation || e->original_btstack_process != 1 ||
        e->mapped_image_hash_verified != 1 || e->owner_thread_verified != 1 ||
        e->callback_serialization_verified != 1 ||
        e->original_iap2_service_observed != 1 ||
        e->rf_arg3_abi_verified != 1 || e->rf_arg3_value < 1 || e->rf_arg3_value > 30 ||
        e->sdp_record_verified != 1 || e->sdp_scn_verified != 1 ||
        e->sdp_scn < 1 || e->sdp_scn > 30 ||
        e->rf_arg3_to_sdp_mapping_verified != 1 ||
        e->callback_bound_to_service != 1 || e->callback_teardown_verified != 1 ||
        e->wired_priority_verified != 1 || e->original_mfi_path_preserved != 1)
        return AU57X_X3_DENY;
    if (e->service_role_known == 1) return AU57X_X3_READY_SERVER;
    if (e->service_role_known == 2) return AU57X_X3_READY_CLIENT;
    return AU57X_X3_DENY;
}
