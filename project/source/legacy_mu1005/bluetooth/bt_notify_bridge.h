#ifndef MU1005_BT_NOTIFY_BRIDGE_H
#define MU1005_BT_NOTIFY_BRIDGE_H
#include "bt_iap2_owner_handoff.h"
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
 int *prepared_fd;
 Au98DispatchPolicy policy;
 Au98OwnerProof proof;
 MuBtOwner owner;
 int (*daemon_verified)(void*,int);
 void *daemon_context;
} MuBtNotifyBinding;
int mu_bt_notify_bind(const MuBtNotifyBinding*);
int mu_bt_notify_unbind(void);
#endif
