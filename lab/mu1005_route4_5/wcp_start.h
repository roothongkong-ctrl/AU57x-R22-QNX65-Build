/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_WCP_START_H
#define MU1005_WCP_START_H
#include "supervised_startup.h"
/* Stable storage in the original btstack process, zero-initialized.
 * Called by the genuine supervisor, never by an unrelated daemon.
 * No automatic activation: the original lifecycle/owner provider must
 * be wired before deployment. Do not free while owner can hold references. */
typedef struct {
 MuDaemonSupervisor daemon;
 MuSupervisedStartup startup;
 MuBtNotifyBinding notify;
 MuBtQueuePulseBinding queue;
 int fd,phase;
} MuWcpStart;
/* Start the actual donor daemon in inherited ACK mode, using existing
 * supervisor/READY protocol, then keep the descriptor for the stock AO.
 * The caller supplies verified binary identity and ACK capability. */
int mu_wcp_start_begin(MuWcpStart*,const MuBtNotifyBinding*,const MuBtQueuePulseBinding*,
 const char*,char*const[],int(*)(void*,const char*),void*,int(*)(void*,const char*),void*);
/* Drive from original startup supervisor. 1=queued, not connected.
 * 0=waiting; -1=terminal/retain resources for certified withdrawal. */
int mu_wcp_start_poll(MuWcpStart*,int(*)(void*),void*);
/* Cancel only before first poll. Once queue bootstrap has begun, the
 * original owner must withdraw and retire; no forced close or kill. */
int mu_wcp_start_cancel_unpublished(MuWcpStart*);
#endif
