#ifndef MU1005_NATIVE_BINDING_H
#define MU1005_NATIVE_BINDING_H
#include "native_event.h"
/* Single resident instance, no self-install or fabricated readiness. Only
 * the original process lifecycle owner is allowed to call these functions. */
typedef struct {
 void *opaque;
 int (*owner_ready)(void *); /* real thread, lifecycle, lock context, verified image */
 int (*transport_ready)(void *); /* authenticated local endpoint ready */
 int (*read_live)(void *,uint32_t,void *,size_t); /* callback-scoped checked read */
 int (*paired_authorized)(void *,uint32_t,const uint8_t[6]);
 /* OEM owner must independently attest an iAP2-specific RFCOMM
  * registration argument. HFP's 7 / SIM's 4 look like channel numbers,
  * NOT proven profile enums or valid iAP2 values. ABI still unverified.
  * 1=verified (arg within 1..30), 0=unverified/deny. Never assume zero. */
 int (*iap2_profile_proven)(void *,uint8_t *profile);
 /* R101: freeze the externally attested arg3 once per original owner
  * generation, before opening TCP or calling any native RF/SDP function.
  * Not proof of correctness of OEM iAP2 SCN by itself. */
 uint8_t rf_arg3_snapshot;
 uint8_t rf_arg3_snapshot_valid;
} MuBtOwner;
/* On QNX ARM32, resolve fixed ABI only for the pinned reference executable.
 * On other hosts returns -1 without reading/calling any native address.
 * fd is already connected to 127.0.0.1:57342; caller owns fd afterwards. */
int mu_bt_native_attach(const MuBtOwner *,int fd);
int mu_bt_native_tick(void);
int mu_bt_native_withdraw(int (*barrier)(void *,const Inbound*),void *);
#endif
