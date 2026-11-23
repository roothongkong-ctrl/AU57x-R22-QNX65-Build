#ifndef AU57X_R108_OEM_PORT_H
#define AU57X_R108_OEM_PORT_H
#include <stdint.h>
#include "r105_owner_gate.h"
/* Evidence-only C boundary: no MU1005 OEM implementation / hook included. */
typedef struct {
 void *context;
 int (*exact_image_live)(void *);
 int (*on_original_owner_thread)(void *);
 int (*get_live_iap2_scn)(void *, uint8_t *);
 int (*paired_phone)(void *);
 int (*mfi_hardware_ready)(void *);
 int (*register_iap2)(void *, uint8_t scn);
 /* Required live service and SDP witness, NOT inferred from return code. */
 int (*published_iap2_service)(void *, uint8_t scn);
 int (*unregister_iap2)(void *);
 int (*callbacks_quiesced)(void *);
} Au108OEM;
typedef enum { AU108_EMPTY=0,AU108_LIVE=1,AU108_POISONED=2,AU108_RETIRED=3 } Au108State;
typedef struct {
 Au108State state;
 Au108OEM owner;
 R105Gate gate;
 R105Evidence evidence;
 uint32_t generation;
} Au108Port;
void au108_init(Au108Port *port);
int au108_bind(Au108Port *port, const Au108OEM *owner, uint32_t generation, int wired_active);
int au108_withdraw(Au108Port *port);
int au108_live(Au108Port *port);
#endif
