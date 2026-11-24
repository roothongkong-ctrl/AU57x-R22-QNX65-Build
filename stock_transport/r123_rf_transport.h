#ifndef R123_RF_TRANSPORT_H
#define R123_RF_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>
#include "wcp_wire.h"
/* Data-plane alternative for original mm-ipod transport. Serialized caller.
 * Zero initialize once. One connected socket/session, pre-authorized by actual external OEM owner.
 * No module export, native startup, type enum, pairing or MFi bypass. */
typedef struct {
 int fd,opened,active,failed;
 uint32_t generation;
 uint8_t peer[6],wire[WC_RF_HEADER+WC_RF_MAX];
 size_t used,target,offset;
} R123Transport;
int r123_transport_open(R123Transport *,int fd,const uint8_t peer[6],uint32_t generation);
/* timeout in ms. Returns byte count, 0 on timeout, -1 on failure/revocation.
 * A successful send means bytes were accepted by local IPC, not RF delivery. */
/* Wait for the authenticated peer CONNECTED frame without consuming payload. */
int r123_transport_wait_connected(R123Transport *,int);
int r123_transport_data_ready(R123Transport *);
int r123_transport_recv(R123Transport *,void *,unsigned,int);
int r123_transport_send(R123Transport *,const void *,unsigned,int);
void r123_transport_revoke(R123Transport *);
#endif
