#ifndef MU1005_CONGO_IAP2_BRIDGE_H
#define MU1005_CONGO_IAP2_BRIDGE_H
/*
 * MU1005 native receiver handoff. R105.040 original iAP2 job contract.
 * Caller must run in the original DIO process on its owner thread, hold
 * an exclusive native lifetime lease and supply a live, original
 * UnicodeString8 and the original EXLAP decision. Never synthesizes
 * authentication or EXLAP permission. Not a standalone preload/installer.
 */
#include <stdint.h>
#include <stddef.h>

#define MU1005_DIO_IAP2_PROXY_OFFSET 0x130u
#define MU1005_DIO_IAP2_SUBMIT_VA 0x124990u
#define MU1005_DIO_IAP2_HIGHLEVEL_VA 0x156c48u
#define MU1005_DIO_IAP2_PROXY_VTABLE 0x1a43c8u

enum mu1005_dio_result {
    MU_DIO_REJECT = -1, MU_DIO_PENDING = 1, MU_DIO_SESSION = 2,
    MU_DIO_UNCERTAIN = -2
};
struct mu1005_dio_request {
    uintptr_t manager;
    const void *native_path;
    uint64_t epoch;
    unsigned exlap_decision; /* OEM isExlapGranted: 1 granted, 2 denied */
};
struct mu1005_dio_job {
    uint64_t epoch, id;
    int state;
};
struct mu1005_dio_native {
    void *ctx;
    /* Validate exact firmware image, original function address and ABI. */
    int (*identity)(void *, uintptr_t, uintptr_t);
    /* Native owner thread, original lease, wired idle, valid native path,
       same current epoch, authentic EXLAP decision. */
    int (*admit)(void *, const struct mu1005_dio_request *);
    int (*read_word)(void *, uintptr_t, uint32_t *);
    /* Existing original queue producer, not a direct driver call. */
    uint64_t (*submit)(void *, const void *, unsigned);
    /* Returns 1 only for an independently authenticated matching session;
       0 pending, negative invalid/lost. */
    int (*session)(void *, uint64_t, uint64_t);
    /* Returns 1 only after the original owner confirms this job retired. */
    int (*retired)(void *, uint64_t, uint64_t);
};
static inline int mu1005_dio_submit_once(struct mu1005_dio_job *j,
        const struct mu1005_dio_native *n, const struct mu1005_dio_request *r)
{
    uintptr_t proxy;
    uint32_t vptr;
    uint64_t id;
    int observed;
    if (!j || !n || !r || !r->epoch || !r->manager || !r->native_path ||
        r->manager > UINT32_MAX - MU1005_DIO_IAP2_PROXY_OFFSET ||
        !n->identity || !n->admit || !n->read_word || !n->submit ||
        !n->session || !n->retired ||
        (r->exlap_decision != 1 && r->exlap_decision != 2))
        return MU_DIO_REJECT;
    if (j->state && (j->epoch != r->epoch || j->state == MU_DIO_UNCERTAIN))
        return MU_DIO_UNCERTAIN;
    if (n->identity(n->ctx, (uintptr_t)n->submit, MU1005_DIO_IAP2_SUBMIT_VA) != 1 ||
        n->admit(n->ctx,r) != 1) {
        if (j->state) j->state=MU_DIO_UNCERTAIN;
        return j->state ? j->state : MU_DIO_REJECT;
    }
    proxy=r->manager+MU1005_DIO_IAP2_PROXY_OFFSET;
    if (n->read_word(n->ctx,proxy,&vptr)!=1 ||
        vptr!=MU1005_DIO_IAP2_PROXY_VTABLE || n->admit(n->ctx,r)!=1) {
        if (j->state) j->state=MU_DIO_UNCERTAIN;
        return j->state ? j->state : MU_DIO_REJECT;
    }
    if (!j->state) {
        j->epoch=r->epoch;
        j->state=MU_DIO_PENDING;
        id=n->submit((void *)proxy,r->native_path,r->exlap_decision==1);
        j->id=id;
        if (!id || n->admit(n->ctx,r)!=1) return j->state=MU_DIO_UNCERTAIN;
    }
    observed=n->session(n->ctx,j->epoch,j->id);
    if (n->admit(n->ctx,r)!=1 || observed<0 || observed>1 ||
        (j->state==MU_DIO_SESSION && observed!=1))
        return j->state=MU_DIO_UNCERTAIN;
    if (observed==1) j->state=MU_DIO_SESSION;
    return j->state;
}
static inline int mu1005_dio_release(struct mu1005_dio_job *j,
        const struct mu1005_dio_native *n)
{
    if (!j || !n || !n->retired) return MU_DIO_REJECT;
    if (!j->state) return 0;
    if (n->retired(n->ctx,j->epoch,j->id)!=1) return MU_DIO_UNCERTAIN;
    j->state=0; j->epoch=0; j->id=0;
    return 0;
}
#endif
