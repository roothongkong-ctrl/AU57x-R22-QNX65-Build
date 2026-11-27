/* GPL-3.0-only. Offline adapter for existing cp_step2 IPC, no OEM export. */
#ifndef ROUTE3_IPC_FINISH_H
#define ROUTE3_IPC_FINISH_H
#include "cp_step2_ipc_link.h"
/* Exclusive worker, same authenticated local endpoint as emit_frame.
 * step consumes input with cp_step2_ipc_feed; 1=progress, 0=no progress,
 * other=failed. Must respect supplied absolute monotonic deadline; must
 * never call SDK or reenter send. No new link/rebind during this call. */
typedef struct {
 struct cp_step2_ipc_link *link;
 void *opaque;
 int (*step)(void *,struct cp_step2_ipc_link *,uint64_t);
 uint64_t (*monotonic_ms)(void *);
 uint64_t deadline_ms;
 unsigned max_steps;
} Route3IpcFinish;
/* Actual byte-count candidate for original transport; no native ABI yet. */
int route3_ipc_send(Route3IpcFinish *,const uint8_t *,size_t);
#endif
