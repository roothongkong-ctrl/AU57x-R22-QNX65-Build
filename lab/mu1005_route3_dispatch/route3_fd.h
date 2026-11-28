/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ROUTE3_FD_H
#define ROUTE3_FD_H
#include "ipc_finish.h"
/* Borrow an existing connected nonblocking stream from its verified owner.
 * No listener, new pairing, authentication implementation or OEM injection.
 * Owner keeps fd and context alive, serialized for all calls. */
typedef struct {
 struct cp_step2_ipc_link link;
 int fd,attached;
 void *ctx;
 int (*proof)(void*,int,const uint8_t*,uint32_t);
 uint8_t tx[CP_STEP2_IPC_MAX];
 size_t used,offset;
} Route3Fd;
int route3_fd_attach(Route3Fd*,int,void*,int(*)(void*,int,const uint8_t*,uint32_t));
/* Absolute CLOCK_MONOTONIC deadline in ms. 1 progress, 0 timeout, -1 lost. */
uint64_t route3_fd_now(void*);
int route3_fd_step(Route3Fd*,uint64_t);
int route3_fd_send(Route3Fd*,const uint8_t*,size_t,uint64_t);
int route3_fd_recv(Route3Fd*,uint8_t*,size_t);
void route3_fd_detach(Route3Fd*);
#endif
