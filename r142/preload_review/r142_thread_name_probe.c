/* R142: read-only thread-name observer for MU1005 research.
 * Build only with -DR142_REVIEW_ONLY=1. No RFCOMM, SDP, iAP2, MFi,
 * AirPlay, Wi-Fi, device-node, or OEM-memory operations are implemented.
 * The only interposed OEM API is pthread_setname_np; every call is
 * forwarded to the next implementation with identical arguments and the
 * original return code is returned unchanged.
 */
#define _POSIX_C_SOURCE 200809L
#ifndef R142_REVIEW_ONLY
#error "R142 is research-only; define R142_REVIEW_ONLY=1 explicitly"
#endif
#include <pthread.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
typedef int (*setname_fn)(pthread_t, const char *);
static setname_fn next_setname;
static void *libc_handle;
static volatile int probe_state;
static volatile int log_lock;
static volatile unsigned seen_bits;
static int log_fd = -1;
static char log_path[256] = "/tmp/au57x_r142_thread_probe.log";
static int has_token(const char *s, const char *token) {
    return s && token && strstr(s, token) != NULL;
}
static int has_btproc_token(const char *name) {
    return name && name[0] && has_token(name + 1, "bluetoothProc");
}
static void lock_log(void) { while (__sync_lock_test_and_set(&log_lock, 1)) { } }
static void unlock_log(void) { __sync_lock_release(&log_lock); }
static size_t append_text(char *dst, size_t cap, size_t at, const char *src) {
    while (src && *src && at + 1 < cap) dst[at++] = *src++;
    if (cap) dst[at < cap ? at : cap - 1] = '\0';
    return at;
}
static size_t append_tid(char *dst, size_t cap, size_t at, pthread_t tid) {
    static const char hex[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)&tid;
    for (size_t i = 0; i < sizeof tid && at + 2 < cap; ++i) {
        dst[at++] = hex[p[i] >> 4]; dst[at++] = hex[p[i] & 15];
    }
    if (cap) dst[at < cap ? at : cap - 1] = '\0';
    return at;
}
static void write_line(const char *role, pthread_t tid, const char *name, int rc) {
    char line[384]; size_t n = 0;
    if (log_fd < 0) return;
    lock_log(); line[0] = '\0';
    n = append_text(line, sizeof line, n, "event=thread_name role=");
    n = append_text(line, sizeof line, n, role);
    n = append_text(line, sizeof line, n, " tid_raw=");
    n = append_tid(line, sizeof line, n, tid);
    n = append_text(line, sizeof line, n, " rc=");
    if (rc == 0) n = append_text(line, sizeof line, n, "0");
    else { char num[24]; (void)snprintf(num, sizeof num, "%d", rc); n = append_text(line, sizeof line, n, num); }
    n = append_text(line, sizeof line, n, " name=");
    if (name) for (size_t i = 0; name[i] && i < 96 && n + 2 < sizeof line; ++i) {
        unsigned char c = (unsigned char)name[i]; line[n++] = (c >= 32 && c <= 126) ? (char)c : '?';
    }
    n = append_text(line, sizeof line, n, "\n");
    ssize_t written = write(log_fd, line, n);
    if (written != (ssize_t)n) log_fd = -1;
    unlock_log();
}
static void resolve_original(void) {
    void *sym = dlsym(RTLD_NEXT, "pthread_setname_np");
    if (!sym) {
        libc_handle = dlopen("libc.so.3", RTLD_NOW | RTLD_LOCAL);
        if (libc_handle) sym = dlsym(libc_handle, "pthread_setname_np");
    }
    if (sym && sizeof next_setname == sizeof sym) memcpy(&next_setname, &sym, sizeof next_setname);
    if (!next_setname) __sync_lock_test_and_set(&probe_state, -1);
}
__attribute__((constructor)) static void r142_init(void) {
    resolve_original();
    if (!next_setname) return;
    const char *enable = getenv("AU57X_R142_THREAD_PROBE");
    if (!enable || strcmp(enable, "1") != 0) return;
    const char *custom = getenv("AU57X_R142_LOG");
    if (custom && custom[0] == '/' && strlen(custom) < sizeof log_path) memcpy(log_path, custom, strlen(custom) + 1);
    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (log_fd < 0) { __sync_lock_test_and_set(&probe_state, -2); return; }
    __sync_lock_test_and_set(&probe_state, 1);
    static const char ready[] = "event=observer_ready mode=read_only target=MU1005_btstack\n";
    ssize_t ready_written = write(log_fd, ready, sizeof ready - 1);
    if (ready_written != (ssize_t)(sizeof ready - 1)) { close(log_fd); log_fd = -1; __sync_lock_test_and_set(&probe_state, -2); }
}
int pthread_setname_np(pthread_t thread, const char *name) {
    if (!next_setname) return ENOSYS;
    int rc = next_setname(thread, name);
    if (__sync_fetch_and_add(&probe_state, 0) != 1 || !name) return rc;
    if (has_btproc_token(name)) {
        __sync_fetch_and_or(&seen_bits, 1u);
        write_line("primary_bluetooth_proc_candidate", thread, name, rc);
    } else if (has_token(name, "mv8787bt_read")) {
        __sync_fetch_and_or(&seen_bits, 2u);
        write_line("secondary_bluesdk_read_worker", thread, name, rc);
    } else if (has_token(name, "mv8787bt_write")) {
        __sync_fetch_and_or(&seen_bits, 4u);
        write_line("secondary_bluesdk_write_worker", thread, name, rc);
    }
    return rc;
}
int r142_thread_probe_status(void) { return __sync_fetch_and_add(&probe_state, 0); }
unsigned r142_thread_probe_seen_mask(void) { return __sync_fetch_and_add(&seen_bits, 0); }
