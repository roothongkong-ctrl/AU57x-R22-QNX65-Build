/* QNX MU1005 receiver integration; owner-thread API, no automatic activation.
 * Uses the existing stock receiver/CPRX implementation and the new iAP2
 * job adapter. Caller supplies independently verified real OEM providers.
 * Keep this object and the service/native storage alive until retired.
 */
#include <stddef.h>
#include "dio_existing_receiver.h"
#include "native_iap2_bridge.h"

typedef struct {
    struct mu1005_dio_job job;
    uint64_t epoch;
    uint32_t generation, session;
    uintptr_t manager;
    const void *path;
    int opened;
} MuCongoReceiver;

int mu_congo_receiver_open(MuCongoReceiver *b, MuCprxService *s,
    MuDioNative *d, void *stock_library, const MuDioGuard *guard,
    const char *original_if, const char *wireless_if, const MuCprxEvidence *e,
    unsigned port, uint32_t generation, uint32_t session, const char *dir,
    uid_t uid, gid_t gid, const struct mu1005_dio_native *ops,
    const struct mu1005_dio_request *request)
{
    int rc;
    if (!b || b->opened || b->job.state || !s || !d || !ops ||
        !ops->admit || !request || !request->manager || !request->native_path ||
        !generation || !session || !request->epoch || request->epoch <= b->epoch ||
        ops->admit(ops->ctx,request)!=1) return -1;
    b->epoch=request->epoch;
    b->generation=generation;
    b->session=session;
    b->manager=request->manager;
    b->path=request->native_path;
    rc=mu_dio_existing_receiver_open(s,d,stock_library,(const void*)b->manager,
        generation,guard,original_if,wireless_if,e,port,b->epoch,session,dir,uid,gid);
    if (rc==-2) { b->opened=-1; return -2; }
    if (rc!=0) return -1;
    b->opened=1;
    if (ops->admit(ops->ctx,request)!=1) return -2;
    return 0; /* listener only, NOT a connected phone */
}

int mu_congo_receiver_tick(MuCongoReceiver *b, const struct mu1005_dio_native *ops,
    const struct mu1005_dio_request *request)
{
    if (!b || b->opened!=1 || !request || request->epoch!=b->epoch ||
        request->manager!=b->manager || request->native_path!=b->path)
        return MU_DIO_REJECT;
    return mu1005_dio_submit_once(&b->job,ops,request);
}

/* Only after independently confirmed original job retirement. No forced
 * release of an original receiver or its outstanding callbacks. */
int mu_congo_receiver_close(MuCongoReceiver *b, MuCprxService *s,
    const struct mu1005_dio_native *ops)
{
    if (!b || !s || b->opened!=1) return -2;
    if (mu1005_dio_release(&b->job,ops)!=0) return -2;
    if (mu_cprx_shutdown(s)!=1) return -2;
    b->opened=0; b->manager=0; b->path=0;
    b->generation=0; b->session=0;
    return 0;
}
