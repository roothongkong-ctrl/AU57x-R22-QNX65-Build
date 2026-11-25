#ifndef MU1005_BT_QUEUE_PULSE_H
#define MU1005_BT_QUEUE_PULSE_H
#include "bt_notify_bridge.h"
/* One immutable reference-counted event; request/run/release may use
 * different threads. Retire only after producer joined and last release. */
typedef struct {volatile unsigned pending,active;} MuBtPulse;
int mu_bt_pulse_request(MuBtPulse*);
int mu_bt_pulse_execute(MuBtPulse*,int(*)(void*),void*);
void mu_bt_pulse_release(MuBtPulse*);
void mu_bt_pulse_disable(MuBtPulse*);
int mu_bt_pulse_idle(MuBtPulse*);
/* Native candidate. Genuine mapped-image proof and a lifetime lease
 * excluding OEM ActiveObject stop/destruction are mandatory. enter/leave
 * may run on producer; no SDK lock held over enqueue. Start/retire on
 * verified owner, stop on supervisor; keep DSO mapped until process exit. */
typedef struct{
 MuBtOwner owner;uint32_t util_load_bias;void *lease_context;
 int (*enter)(void*);void (*leave)(void*);
} MuBtQueuePulseBinding;
int mu_bt_queue_pulse_start(const MuBtQueuePulseBinding*);
/* Start after original BluetoothServices returns initialized. The startup
 * witness certifies the caller's genuine startup context, not AO ownership.
 * First queued event binds the supplied notify context ON the actual AO.
 * All referenced context storage must remain valid until retirement. */
int mu_bt_queue_bootstrap(const MuBtQueuePulseBinding*,const MuBtNotifyBinding*,int(*startup_authorized)(void*),void*);
int mu_bt_queue_pulse_stop(void);
int mu_bt_queue_pulse_retire(void);
#endif
