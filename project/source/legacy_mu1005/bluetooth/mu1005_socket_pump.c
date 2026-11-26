#include "mu1005_socket_pump.h"
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

static AuSocketResult lost(AuIpcStream *stream, AuSocketResult status) {
    au_ipc_socket_lost(stream);
    return status;
}

AuSocketResult au_socket_step(AuIpcStream *stream, int fd, int timeout_ms) {
    if (!stream || !stream->rf || stream->failed || fd < 0 || timeout_ms < 0)
        return AU_SOCKET_IO_ERROR;
    /* Keep the SDK-owned buffer until real send completion; avoid idle
     * POLLOUT loops by requesting writable events only when data exists. */
    if (au_ipc_pump(stream)) return lost(stream, AU_SOCKET_IO_ERROR);
    const unsigned char *ready_bytes = 0;
    size_t ready_length = 0;
    int ready = au_ipc_peek(stream, &ready_bytes, &ready_length);
    if (ready < 0) return lost(stream, AU_SOCKET_IO_ERROR);
    /* TCP can coalesce arbitrarily many small DATA records. While BlueSDK
     * owns an asynchronous TX descriptor, stop reading before the bounded
     * pending queue fills. TCP supplies the backpressure; do not drop a
     * valid record just because the daemon is faster than RFCOMM.
     * Minimum normalized record is 8-byte header + 1 payload byte.
     * An existing partial record consumes one of the remaining slots. */
    size_t capacity = 0;
    if (stream->pending_count < AU_RF_QSIZE) {
        size_t slots = AU_RF_QSIZE - stream->pending_count;
        size_t total = stream->expected ? stream->expected : 9u;
        if (stream->used >= total)
            return lost(stream, AU_SOCKET_IO_ERROR);
        capacity = total - stream->used + (slots - 1u) * 9u;
    }
    struct pollfd pfd = { fd, (short)((capacity ? POLLIN : 0) |
        ((ready > 0 && ready_length) ? POLLOUT : 0)), 0 };
    int pr;
    do { pr = poll(&pfd, 1, timeout_ms); } while (pr < 0 && errno == EINTR);
    if (pr < 0) return lost(stream, AU_SOCKET_IO_ERROR);
    if (pr == 0) return AU_SOCKET_WAIT;
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        return lost(stream, AU_SOCKET_CLOSED);

    int progressed = 0;
    if (pfd.revents & POLLIN) {
        unsigned char buf[2048];
        ssize_t n;
        size_t limit = capacity < sizeof(buf) ? capacity : sizeof(buf);
        do { n = recv(fd, buf, limit, 0); } while (n < 0 && errno == EINTR);
        if (n == 0) return lost(stream, AU_SOCKET_CLOSED);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return lost(stream, AU_SOCKET_IO_ERROR);
        } else {
            if (au_ipc_feed(stream, buf, (size_t)n))
                return lost(stream, AU_SOCKET_IO_ERROR);
            progressed = 1;
        }
    }
    if (pfd.revents & POLLOUT) {
        const unsigned char *bytes = 0;
        size_t len = 0;
        int peek = au_ipc_peek(stream, &bytes, &len);
        if (peek < 0) return lost(stream, AU_SOCKET_IO_ERROR);
        if (peek > 0 && len) {
            /* No SIGPIPE may escape into an OEM parent process. */
#ifdef MSG_NOSIGNAL
            int flags = MSG_NOSIGNAL;
#else
            /* The caller must configure no-SIGPIPE behavior at socket setup. */
            int flags = 0;
#endif
            ssize_t n;
            do { n = send(fd, bytes, len, flags); } while (n < 0 && errno == EINTR);
            if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    return lost(stream, AU_SOCKET_IO_ERROR);
            } else if (n > 0) {
                if (au_ipc_commit(stream, (size_t)n))
                    return lost(stream, AU_SOCKET_IO_ERROR);
                progressed = 1;
            }
        }
    }
    return progressed ? AU_SOCKET_PROGRESS : AU_SOCKET_WAIT;
}

AuSocketResult au_socket_tx_complete(AuIpcStream *stream, int sdk_success) {
    if (!stream || !stream->rf || stream->failed) return AU_SOCKET_IO_ERROR;
    if (au_rf_send_complete(stream->rf, sdk_success))
        return lost(stream, AU_SOCKET_IO_ERROR);
    if (au_ipc_pump(stream)) return lost(stream, AU_SOCKET_IO_ERROR);
    return AU_SOCKET_PROGRESS;
}
