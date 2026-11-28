/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_SUPERVISED_NOTIFY_H
#define MU1005_SUPERVISED_NOTIFY_H
#include "daemon_supervisor.h"
#include "bt_notify_bridge.h"
/* Call only on startup supervisor BEFORE publishing the immutable binding
 * to the original AO. No concurrent access until handoff/reap is finished.
 * Caller owns storage and must verify exact executable AND ACK protocol
 * capability independently. This does not supply OEM lifecycle authority. */
int mu_supervised_notify_prepare(MuDaemonSupervisor*,MuBtNotifyBinding*,
 const char*,char*const[],int(*)(void*,const char*),void*,
 int(*)(void*,const char*),void*);
#endif
