#ifndef MU1005_NATIVE_BINDING_H
#define MU1005_NATIVE_BINDING_H
#include "native_event.h"
#include "../../x3/src/mu1005_owner_gate.h"
#include <stdint.h>
#include <stddef.h>
typedef struct {
 void *opaque;
 int (*owner_ready)(void *);
 int (*transport_ready)(void *);
 int (*read_live)(void *,uint32_t,void *,size_t);
 int (*paired_authorized)(void *,uint32_t,const uint8_t[6]);
 int (*iap2_profile_proven)(void *,uint8_t *profile);
 int (*x3_owner_evidence)(void *,Au57xX3Evidence *);
 uint8_t rf_arg3_snapshot;
 uint8_t rf_arg3_snapshot_valid;
} MuBtOwner;
int mu_bt_native_attach(const MuBtOwner *,int fd);
int mu_bt_native_tick(void);
int mu_bt_native_withdraw(int (*barrier)(void *,const Inbound*),void *);
#endif
