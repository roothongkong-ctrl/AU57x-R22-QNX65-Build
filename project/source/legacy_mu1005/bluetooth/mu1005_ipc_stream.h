#ifndef MU1005_IPC_STREAM_H
#define MU1005_IPC_STREAM_H
#include "mu1005_rf_ingress.h"
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
/* Bounded normalized socket bytes, not the proprietary OEM SDK ABI. */
typedef struct {
 AuRfIngress *rf;
 uint8_t incoming[AU_RF_WIRE_MAX];
 size_t used,expected;
 AuRfWire outgoing;
 size_t offset;
 AuRfWire pending[AU_RF_QSIZE];
 unsigned pending_head,pending_tail,pending_count;
 int out_pending,failed;
 uint32_t rx_frames_committed;
} AuIpcStream;
void au_ipc_init(AuIpcStream*,AuRfIngress*);
int au_ipc_feed(AuIpcStream*,const uint8_t*,size_t);
int au_ipc_pump(AuIpcStream*);
int au_ipc_peek(AuIpcStream*,const uint8_t**,size_t*);
int au_ipc_commit(AuIpcStream*,size_t);
void au_ipc_socket_lost(AuIpcStream*);
#endif
