#ifndef AU57X_R105_OWNER_GATE_H
#define AU57X_R105_OWNER_GATE_H
#include <stdint.h>
#define R105_SCN_MIN 1u
#define R105_SCN_MAX 30u
typedef enum { R105_EMPTY=0, R105_ARMED=1, R105_REVOKED=2 } R105GateState;
typedef struct {
 uint32_t generation;
 uint8_t scn;
 uint8_t firmware_verified;
 uint8_t owner_thread_verified;
 uint8_t iap2_service_verified;
 uint8_t paired_authorized;
 uint8_t original_mfi_provider_verified;
 uint8_t wired_active;
} R105Evidence;
typedef struct { R105GateState state; uint32_t generation; uint8_t scn; } R105Gate;
void r105_gate_init(R105Gate *g);
int r105_gate_arm(R105Gate *g,const R105Evidence *e);
int r105_gate_allow(const R105Gate *g,const R105Evidence *e);
void r105_gate_revoke(R105Gate *g);
#endif
