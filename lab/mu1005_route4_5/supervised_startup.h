/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_SUPERVISED_STARTUP_H
#define MU1005_SUPERVISED_STARTUP_H
#include "supervised_notify.h"
#include "bt_startup_watch.h"
/* Supervisor-only startup coordinator. All input bindings must stay valid
 * and unchanged. 0 waiting; 1 queued (NOT registered); -1 terminal.
 * After queue publication, never inspect or close the owner's FD here.
 * On any ambiguous failure, retain resources for certified withdrawal.
 * No OEM lifecycle lease or startup authority is invented by this module. */
typedef struct {
 MuBtStartupWatch watch;
 unsigned remaining;
 int phase;
} MuSupervisedStartup;
int mu_supervised_startup_tick(MuSupervisedStartup*,MuDaemonSupervisor*,
 const MuBtNotifyBinding*,const MuBtQueuePulseBinding*,int(*)(void),
 unsigned total_ticks,unsigned trace_ticks,int(*)(void*),void*);
#endif
