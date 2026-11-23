#ifndef AU57X_R108_OEM_PORT_H
#define AU57X_R108_OEM_PORT_H
#include <stdint.h>
#include "r105_owner_gate.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Source-only ABI contract; callbacks MUST be implemented in the verified
 * MU1005 btstack owner's serialized context. No injection or installer. */
typedef struct {
 void *context;
 int (*exact_image_live)(void *);
 int (*on_original_owner_thread)(void *);
 int (*paired_phone)(void *);
 int (*mfi_hardware_ready)(void *);
 int (*wired_idle)(void *);
 /* Native SDK registration ALLOCATES and RETURNS SCN, it is NOT a prerequisite.
  * 0 = success; failure leaves storage and registration state ambiguous. */
 int (*register_iap2_rfcomm)(void *, uint8_t *assigned_scn);
 /* Advertise exact allocated SCN only AFTER RF registration. 0=success. */
 int (*publish_iap2_sdp)(void *, uint8_t assigned_scn);
 /* Independent live witness: RF channel + iAP SDP with identical SCN. */
 int (*published_iap2_service)(void *, uint8_t assigned_scn);
 /* Withdraw only on original serialized owner with no pending callbacks. */
 int (*unregister_iap2)(void *);
 int (*callbacks_quiesced)(void *);
} Au108OEM;
typedef enum { AU108_EMPTY=0,AU108_LIVE=1,AU108_POISONED=2,AU108_RETIRED=3,AU108_SUSPENDED=4,AU108_ADVERTISING=5 } Au108State;
typedef struct {
 Au108State state;
 Au108OEM owner;
 R105Gate gate;
 R105Evidence evidence;
 uint32_t generation;
} Au108Port;
/* SUSPENDED blocks any further wireless use but preserves native registration
 * until its genuine serialized OEM owner can safely withdraw it. */
void au108_init(Au108Port *port);
int au108_bind(Au108Port *port,const Au108OEM *owner,uint32_t generation);
int au108_live(Au108Port *port);
int au108_withdraw(Au108Port *port);
#ifdef __cplusplus
}
#endif
#endif
