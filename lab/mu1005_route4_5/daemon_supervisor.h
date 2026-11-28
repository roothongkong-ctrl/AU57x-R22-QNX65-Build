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
 int active,reaped;
} MuDaemonSupervisor;
int mu_daemon_supervisor_start(MuDaemonSupervisor*,const char*,char*const[],
 int(*)(void*,const char*),void*);
int mu_daemon_supervisor_live(MuDaemonSupervisor*,int);
int mu_daemon_supervisor_close(MuDaemonSupervisor*);
#endif
