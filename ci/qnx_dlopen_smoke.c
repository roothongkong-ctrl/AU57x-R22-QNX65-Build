/* QNX 6.5 ARMv7: test loader only, never establish an OEM session. */
#include <stdio.h>
#include <dlfcn.h>

int main(void) {
    const char *name = "libau57x_r22_native_qnx_REVIEW_ONLY.so";
    void *h = dlopen(name, RTLD_NOW);
    if (!h) {
        const char *err = dlerror();
        printf("AU57X_DLOPEN_FAILED: %s\n", err ? err : "unknown");
        return 11;
    }
    typedef int (*ready_fn)(const void *);
    dlerror();
    ready_fn f = (ready_fn)dlsym(h, "cp_ho_media_ready");
    const char *err = dlerror();
    if (err || !f) {
        printf("AU57X_DLSYM_FAILED: %s\n", err ? err : "not found");
        dlclose(h);
        return 12;
    }
    /* NULL must be rejected by the review-only state handler. */
    if (f(NULL) != 0) {
        puts("AU57X_SAFE_NULL_REJECT_FAILED");
        dlclose(h);
        return 13;
    }
    if (dlclose(h) != 0) {
        puts("AU57X_DLCLOSE_FAILED");
        return 14;
    }
    puts("AU57X_RUNTIME_DLOPEN_OK");
    return 0;
}
