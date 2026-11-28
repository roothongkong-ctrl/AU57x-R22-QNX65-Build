/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_DAEMON_SUPERVISOR_H
#define MU1005_DAEMON_SUPERVISOR_H
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
/* Worker-only review candidate. Caller must independently certify the exact
 * executable and protected immutable deployment directory, and own child
 * reaping. Not a cross-process btstack owner or MFi proof. */
typedef struct {
 int fd;pid_t pid;struct stat identity;struct sockaddr_in local,peer;
 int active,reaped,transferred;
} MuDaemonSupervisor;
int mu_daemon_supervisor_start(MuDaemonSupervisor*,const char*,char*const[],
 int(*)(void*,const char*),void*);
int mu_daemon_supervisor_live(MuDaemonSupervisor*,int);
/* Transfer the FD exactly once to the serialized owner mailbox. The
 * supervisor never closes that descriptor again. Caller closes on rejected
 * adoption or via certified handoff withdrawal; no concurrent access. */
int mu_daemon_supervisor_offer(MuDaemonSupervisor*,int*);
int mu_daemon_supervisor_witness(void*,int);
/* Only the original serialized supervisor, before AO publication. */
int mu_daemon_supervisor_retract_unpublished(MuDaemonSupervisor*,int);
int mu_daemon_supervisor_close(MuDaemonSupervisor*);
#endif
