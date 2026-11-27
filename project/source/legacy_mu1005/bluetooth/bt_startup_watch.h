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
#endif
