#ifndef MU1005_BT_NOTIFY_BRIDGE_H
#define MU1005_BT_NOTIFY_BRIDGE_H
#include "bt_iap2_owner_handoff.h"
#include "bt_socket_worker.h"
/* Original notify is always called, including wrong-thread and nested
 * calls. Only the outermost original owner callback drives wireless. */
typedef void (*MuNotifyOriginal)(void*,unsigned char);
typedef struct {
 void *opaque;
 int (*on_owner)(void*);
 int (*enabled)(void*);
 void (*after)(void*);
 unsigned inside;
} MuNotifyGate;
void mu_notify_call(MuNotifyGate*,MuNotifyOriginal,void*,unsigned char);
/* Experimental QNX owner-only binding. Caller owns all referenced storage,
 * must prove mapped images, SDK lock exclusion and daemon identity.
 * No automatic hook installation. Keep DSO mapped until process exit. */
typedef struct {
 Au98Handoff *handoff;
 int *prepared_fd; /* owner-only storage, initially -1 */
 MuBtSocketWorker *worker; /* optional supervisor-started producer */
 Au98DispatchPolicy policy;
 Au98OwnerProof proof;
 MuBtOwner owner;
 int (*daemon_verified)(void*,int);
 void *daemon_context;
} MuBtNotifyBinding;
int mu_bt_notify_bind(const MuBtNotifyBinding*);
/* Queue-only mode leaves original notification slot unchanged. */
int mu_bt_notify_bind_queue_only(const MuBtNotifyBinding*);
int mu_bt_notify_discovery_allowed(void);
int mu_bt_notify_periodic_dispatch(void);
int mu_bt_notify_unbind(void);
#endif
