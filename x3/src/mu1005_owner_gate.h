#ifndef AU57X_X3_MU1005_OWNER_GATE_H
#define AU57X_X3_MU1005_OWNER_GATE_H
#include <stdint.h>
typedef struct {
    const char *btstack_sha256;
    uint32_t owner_generation;
    uint8_t original_btstack_process;
    uint8_t mapped_image_hash_verified;
    uint8_t owner_thread_verified;
    uint8_t callback_serialization_verified;
    uint8_t original_iap2_service_observed;
    uint8_t service_role_known;
    uint8_t rf_arg3_abi_verified;
    uint8_t rf_arg3_value;
    uint8_t sdp_record_verified;
    uint8_t sdp_scn_verified;
    uint8_t sdp_scn;
    uint8_t rf_arg3_to_sdp_mapping_verified;
    uint8_t callback_bound_to_service;
    uint8_t callback_teardown_verified;
    uint8_t wired_priority_verified;
    uint8_t original_mfi_path_preserved;
} Au57xX3Evidence;
enum { AU57X_X3_DENY=-1, AU57X_X3_READY_SERVER=1, AU57X_X3_READY_CLIENT=2 };
int au57x_x3_owner_gate(const Au57xX3Evidence *e);
#endif
