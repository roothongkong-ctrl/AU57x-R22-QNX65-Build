/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_BT_STARTUP_WATCH_H
#define MU1005_BT_STARTUP_WATCH_H
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include "bt_trace_latch.h"
typedef struct {unsigned remaining;} MuBtStartupWatch;
/* Worker-only. Each tick consumes at most one observation under a caller-
 * provided existing lifetime lease. No pointer escapes the lease; ready()
 * is a notification, NOT a service registration or owner proof.
 * ticks is a bounded retry budget, not wall-clock time. */
int mu_bt_startup_watch_tick(MuBtStartupWatch*,MuBtTraceLatch*,unsigned ticks,
 int(*enter)(void*),void(*leave)(void*),
 int(*read_live)(void*,uint32_t,void*,size_t),
 void(*ready)(void*,uint32_t),void*);
/* Source callback permits the existing preload's real attached trace
 * provider to feed the watcher without exporting its private latch.
 * NULL source polls once per tick under the existing lease; caller must
 * enforce a finite total deadline. No trace or ownership is fabricated. */
int mu_bt_startup_watch_tick_source(MuBtStartupWatch*,int(*take)(void*),void*,unsigned,
 int(*enter)(void*),void(*leave)(void*),int(*read_live)(void*,uint32_t,void*,size_t),
 void(*ready)(void*,uint32_t),void*);
#include "bt_queue_pulse.h"
/* Worker-only, real stock lifecycle providers must be supplied by the
 * caller. trace_take may be NULL for bounded supervisor polling. No automatic activation. 1 means bootstrap
 * queued, not a registered service. 0 pending; -1 failure. */
int mu_bt_startup_bootstrap_tick(MuBtStartupWatch*,int(*trace_take)(void),unsigned,
 const MuBtQueuePulseBinding*,const MuBtNotifyBinding*,int(*startup)(void*),void*);
#endif
