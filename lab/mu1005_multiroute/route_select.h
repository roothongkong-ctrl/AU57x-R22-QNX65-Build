/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_ROUTE_SELECT_H
#define MU1005_ROUTE_SELECT_H
#include <stdint.h>
enum mu_route { MU_ROUTE_NONE=0, MU_ROUTE_DIRECT=1, MU_ROUTE_SI=2, MU_ROUTE_TYPE3=4 };
enum mu_route_state { MU_ROUTE_IDLE=0, MU_ROUTE_TRIAL, MU_ROUTE_CONNECTED, MU_ROUTE_RESTORE, MU_ROUTE_WIRED, MU_ROUTE_EXHAUSTED };
typedef struct {
 unsigned enabled,failed,active,preferred;
 uint64_t epoch,last_epoch;
 enum mu_route_state state;
 int wired_pending;
} MuRouteSelect;
/* A/B are receiver launch alternatives. C is a Type-3 ingress + shared
 * original receiver candidate, not an independent foreign CarPlay binary.
 * ready_mask must come from the real respective provider, never CI results.
 * Every new trial receives a fresh epoch after full original restoration. */
void mu_route_init(MuRouteSelect*,unsigned enabled,unsigned preferred);
unsigned mu_route_begin(MuRouteSelect*,uint64_t epoch,unsigned ready_mask,int wired_idle);
int mu_route_result(MuRouteSelect*,unsigned route,uint64_t epoch,int result,int wireless_verified);
int mu_route_usb(MuRouteSelect*);
int mu_route_restored(MuRouteSelect*,unsigned route,uint64_t epoch,int original_restored,int wired_idle);
#endif
