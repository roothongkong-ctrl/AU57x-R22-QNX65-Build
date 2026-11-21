#ifndef MU1005_SOCKET_PUMP_H
#define MU1005_SOCKET_PUMP_H
#include "mu1005_ipc_stream.h"
/* QNX/POSIX socket adapter. Nonblocking fd owned by caller. Call only on the
 * same serialized worker as SDK callbacks; no OEM function addresses. */
typedef enum {
    AU_SOCKET_WAIT = 0,
    AU_SOCKET_PROGRESS = 1,
    AU_SOCKET_CLOSED = -1,
    AU_SOCKET_IO_ERROR = -2
} AuSocketResult;
AuSocketResult au_socket_step(AuIpcStream *stream, int fd, int timeout_ms);
AuSocketResult au_socket_tx_complete(AuIpcStream *stream, int sdk_success);
#endif
