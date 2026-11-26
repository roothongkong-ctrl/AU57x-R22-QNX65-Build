#ifndef MU1005_NATIVE_BINDING_H
#define MU1005_NATIVE_BINDING_H
#include "native_event.h"
/* Single resident instance, no self-install or fabricated readiness. Only
 * the original process lifecycle owner is allowed to call these functions. */
typedef struct {
 void *opaque;
 int (*owner_ready)(void *); /* real thread, lifecycle, lock context, verified image */
 int (*transport_ready)(void *); /* authenticated local endpoint ready */
 int (*read_live)(void *,uint32_t,void *,size_t); /* checked executable/object/peer reads, scoped to verified lifetime */
 int (*paired_authorized)(void *,uint32_t,const uint8_t[6]);
 /* Optional explicit initial RX credits 1..7, not SCN. NULL selects the
  * pinned MU1005 stock four-credit first-connect parameter in R98. */
 int (*iap2_profile_proven)(void *,uint8_t *profile);
 /* R101: freeze the externally attested arg3 once per original owner
  * generation, before opening TCP or calling any native RF/SDP function.
  * Not proof of live OEM iAP2 readiness. */
 uint8_t rf_arg3_snapshot;
 uint8_t rf_arg3_snapshot_valid;
 /* Actual BluetoothServices this pointer captured by a verified owner. */
 uint32_t services_address;
 /* Independently authenticated mapped original libcomm.so load bias.
  * read_live must also support checked executable reads at attach. */
 uint32_t comm_load_bias;
} MuBtOwner;
/* On QNX ARM32, resolve fixed ABI only for the pinned reference executable.
 * On other hosts returns -1 without reading/calling any native address.
 * fd is already connected to 127.0.0.1:57342; caller owns fd afterwards. */
int mu_bt_native_attach(const MuBtOwner *,int fd);
int mu_bt_native_tick(void);
int mu_bt_native_discovery_ready(void);
int mu_bt_native_eir_buffer_matches(const uint8_t*);
/* Review-only replacement for exact original BL at 0x12a694.
 * Never install or call outside the authenticated stock call site. */
unsigned mu_bt_native_eir_builder_review(uint8_t*,unsigned);
/* Queue original EIR refresh on certified SDK owner. Not a controller ACK. */
int mu_bt_native_refresh_eir(void);
/* Serialized original owner only; no hook installed by this API. */
int mu_bt_native_append_eir(uint8_t*,size_t,size_t,int (*)(void*),void*,size_t*);
int mu_bt_native_withdraw(int (*barrier)(void *,const Inbound*),void *);
int mu_bt_native_reclaim(int (*reclaimed)(void*,const Inbound*),void*);
#endif
