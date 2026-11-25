#ifndef AU57X_R98_BT_IAP2_OWNER_HANDOFF_H
#define AU57X_R98_BT_IAP2_OWNER_HANDOFF_H
/* R98: only a verified original iAP2 SDK owner can publish the stock-derived
 * RFCOMM service into a real 127.0.0.1:57342 control-daemon socket.
 * A successful publication does NOT mean a handset, iAP2 or MFi is connected.
 */
#include "native_binding.h"
#include "bt_route_gate.h"
#include <stdint.h>

typedef enum {
 AU98_EMPTY=0, AU98_PUBLISHED=1, AU98_RETIRED=2, AU98_POISONED=3,
 AU98_DRAINING=4 /* successful attach, transport stopped; cleanup only */
} Au98State;

typedef struct {
 void *opaque;
 int (*original_image_verified)(void *); /* actual original btstack running */
 int (*on_original_iap2_owner)(void *); /* original serialized iAP2 owner, not HFP */
 int (*original_iap2_service_verified)(void *); /* actual original service role */
 int (*attach)(void *,const MuBtOwner *,int); /* QNX: mu_bt_native_attach */
 int (*tick)(void *); /* QNX: mu_bt_native_tick */
 /* -1 = preconditions not met, no SDK mutation; -2 = uncertain mutation. */
 int (*withdraw)(void *,int (*barrier)(void*,const Inbound*),void*);
} Au98Hooks;

typedef struct {
 Au98State state;
 int fd;
 uint32_t generations;
 Au98Hooks hooks;
 MuBtOwner native_owner;
} Au98Handoff;

/* port is ALWAYS 57342 for deployment. An ephemeral port override exists
 * only in a host fixture built with AU57X_R98_HOST_TEST. */
int au98_publish(Au98Handoff*,const Au98Hooks*,const MuBtOwner*,unsigned port);
/* Worker transfers exclusive prepared nonblocking fd; -1 caller owns,
 * 0/-2 handoff owns. Exact daemon attestation must be nonblocking. */
int au98_adopt(Au98Handoff*,const Au98Hooks*,const MuBtOwner*,int,unsigned,int(*)(void*,int),void*);
/* Serialized original owner only. Any lost owner/transport proof on that
 * thread enters DRAINING; recovering proof permits withdrawal, not tick.
 * Wrong-thread calls are rejected without mutating the owner state. */
int au98_tick(Au98Handoff*);
int au98_withdraw(Au98Handoff*,int (*barrier)(void*,const Inbound*),void*);
/* Concrete QNX adapter to existing native_binding.c, not a fake callback.
 * Proof providers are NOT supplied by the bridge: the genuine stock iAP2
 * service owner must supply them from its real SDK/session lifecycle. */
typedef struct {
 void *opaque;
 int (*original_image_verified)(void*);
 int (*on_original_iap2_owner)(void*);
 int (*original_iap2_service_verified)(void*);
 /* R100 explicit software-graft evidence; no endpoint/owner inference from HFP. */
 const Au100Evidence *route_evidence;
} Au98OwnerProof;
/* Legacy connect path may wait; never call from SDK event dispatch. */
int au98_bind_stock_native(Au98Handoff*,const Au98OwnerProof*,const MuBtOwner*);
int au98_adopt_stock_native(Au98Handoff*,const Au98OwnerProof*,const MuBtOwner*,int,int(*)(void*,int),void*);
/* After clean withdrawal and independent proof no old SDK callbacks can
 * run, release storage for a fresh publication. No automatic retry. */
int au98_rearm_stock_native(Au98Handoff*,int(*reclaimed)(void*,const Inbound*),void*);

#endif
