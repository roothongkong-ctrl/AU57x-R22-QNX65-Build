/* QNX 6.5 ARMv7: test loader only, never establish an OEM session. */
#include <stdio.h>
#include <dlfcn.h>
#include <stddef.h>

int main(void) {
    const char *name = "libau57x_r23_native_qnx_REVIEW_ONLY.so";
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
    typedef int (*csm_fn)(const unsigned char *, size_t);
    typedef int (*iface_fn)(const char *, char *, size_t);
    dlerror();
    csm_fn validate = (csm_fn)dlsym(h, "mu_sem_csm");
    iface_fn iface = (iface_fn)dlsym(h, "mu_wifi_interface_for_ipv4");
    void *service = dlsym(h, "mu_cprx_open_backend_si");
    err = dlerror();
    if (err || !validate || !iface || !service) {
        printf("AU57X_R23_MODULE_SYMBOL_MISSING: %s\n", err ? err : "empty");
        dlclose(h);
        return 15;
    }
    const unsigned char csm[] = {0x40, 0x40, 0x00, 0x06, 0x50, 0x00};
    if (validate(csm, sizeof csm) != 1 || validate(csm, 5) != 0) {
        puts("AU57X_R23_CSM_REJECTION_FAILED");
        dlclose(h);
        return 16;
    }
    char interface_name[17];
    if (iface("127.0.0.1", interface_name, sizeof interface_name) != -1) {
        puts("AU57X_R23_LOOPBACK_REJECTION_FAILED");
        dlclose(h);
        return 17;
    }
    puts("AU57X_R23_MODULES_LOADED_OK");
    if (dlclose(h) != 0) {
        puts("AU57X_DLCLOSE_FAILED");
        return 14;
    }
    puts("AU57X_RUNTIME_DLOPEN_OK");
    return 0;
}
