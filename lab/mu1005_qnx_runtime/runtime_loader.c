/* QNX ARM guest proof, exact immutable IFS bytes, no OEM attach. */
#if !defined(__QNXNTO__) || !defined(__arm__)
#error QNX ARM target required
#endif
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "native_blob.h"

static void stage(const char *s) {
    fprintf(stderr, "MU1005_STAGE:%s\n", s);
    fflush(stderr);
}
static int fail(const char *s) {
    int e = errno;
    fprintf(stderr, "MU1005_FAIL:%s errno=%d %s\n", s, e, strerror(e));
    fflush(stderr);
    return 1;
}
static int verify_file(const char *path) {
    unsigned char b[1024];
    size_t off = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return fail("open IFS library");
    while (off < sizeof native_blob) {
        ssize_t n = read(fd, b, sizeof b);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0 || (size_t)n > sizeof native_blob - off) {
            fprintf(stderr, "MU1005_READ_FAIL offset=%lu got=%ld expected_total=%lu errno=%d\n",
                    (unsigned long)off, (long)n, (unsigned long)sizeof native_blob, errno);
            close(fd); errno = EIO; return fail("IFS short read");
        }
        if (memcmp(b, native_blob + off, (size_t)n)) {
            size_t i=0;while(i<(size_t)n && b[i]==native_blob[off+i]) ++i;
            fprintf(stderr, "MU1005_MISMATCH offset=%lu expected=%02x actual=%02x\n",
                    (unsigned long)(off+i), (unsigned)native_blob[off+i], (unsigned)b[i]);
            close(fd); errno = EIO; return fail("IFS bytes mismatch");
        }
        off += (size_t)n;
    }
    ssize_t n;
    do { n = read(fd, b, 1); } while (n < 0 && errno == EINTR);
    if (n != 0) { close(fd); errno = EIO; return fail("IFS trailing bytes/read"); }
    if (close(fd)) return fail("close IFS library");
    puts("MU1005_IFS_EXACT_BYTES_OK"); fflush(stdout);
    return 0;
}
int main(void) {
    struct utsname u;
    const char *path = "/proc/boot/libmu1005_bt_native_review.so";
    static const char *const syms[] = {
        "mu_bt_native_attach", "mu_bt_native_tick", "mu_bt_native_withdraw",
        "inbound_publish", "ir5_publish"
    };
    stage("main entered");
    if (uname(&u) == 0) {
        fprintf(stderr, "QNX_KERNEL=%s %s %s\n", u.sysname, u.release, u.machine);
        fflush(stderr);
        if (strcmp(u.sysname, "QNX") || strncmp(u.release, "6.5", 3)) {
            errno = EINVAL; return fail("unexpected kernel");
        }
    } else {
        /* Minimal IFS may lack hostname services. The locked QNX boot inputs,
           serial boot markers and kernel process exit are independent evidence. */
        fprintf(stderr, "MU1005_UNAME_UNAVAILABLE errno=%d\n", errno);
        fflush(stderr);
    }
    stage("verify read-only IFS library");
    if (verify_file(path)) return 1;
    stage("RTLD_NOW dlopen");
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char *e = dlerror();
        fprintf(stderr, "MU1005_DLOPEN_FAIL:%s\n", e ? e : "no error text");
        fflush(stderr); return 1;
    }
    stage("dlsym");
    for (unsigned i=0; i<sizeof syms/sizeof syms[0]; ++i) {
        (void)dlerror();
        void *p = dlsym(h, syms[i]);
        const char *e = dlerror();
        if (!p || e) {
            fprintf(stderr, "MU1005_DLSYM_FAIL:%s %s\n", syms[i], e ? e : "");
            fflush(stderr); (void)dlclose(h); return 1;
        }
        printf("DLSYM_OK:%s\n", syms[i]); fflush(stdout);
    }
    stage("dlclose");
    if (dlclose(h)) return fail("dlclose");
    puts("MU1005_QNX65_NATIVE_DLOPEN_OK_NO_OEM_ATTACH"); fflush(stdout);
    return 0;
}
