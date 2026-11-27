/* GPL-3.0-only. Research adapter; not an exported mm-ipod ABI. */
#ifndef ROUTE3_BYTE_CONTRACT_H
#define ROUTE3_BYTE_CONTRACT_H
#include "cp_step2_transport.h"
/* Called only on a serialized worker, never on an SDK callback or while
 * holding a lock required by TX completion. The external owner must keep
 * transport and bridge storage alive and exclude rebind/rearm throughout.
 * finish must independently attest the exact current submission's actual
 * byte completion (not mere acceptance), within a bounded deadline.
 * No real MU1005 finish provider has yet been identified. */
typedef int (*route3_finish)(void *,struct cp_step2_transport *,uint32_t,size_t);
int route3_send(struct cp_step2_transport *,const uint8_t *,size_t,route3_finish,void *);
int route3_recv_now(struct cp_step2_transport *,uint8_t *,size_t);
#endif
