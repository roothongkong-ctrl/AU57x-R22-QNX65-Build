#ifndef AU57X_R98_BT_IAP2_OWNER_HANDOFF_H
#define AU57X_R98_BT_IAP2_OWNER_HANDOFF_H
#include "native_binding.h"
#include "bt_route_gate.h"
#include <stdint.h>
typedef enum { AU98_EMPTY=0, AU98_PUBLISHED=1, AU98_RETIRED=2, AU98_POISONED=3 } Au98State;
typedef struct {
 void *opaque;
 int (*original_image_verified)(void *);
 int (*on_original_iap2_owner)(void *);
 int (*original_iap2_service_verified)(void *);
 int (*attach)(void *,const MuBtOwner *,int);
 int (*tick)(void *);
 int (*withdraw)(void *,int (*barrier)(void*,const Inbound*),void*);
} Au98Hooks;
typedef struct {
 Au98State state;
 int fd;
 uint32_t generations;
 Au98Hooks hooks;
 MuBtOwner native_owner;
} Au98Handoff;
int au98_publish(Au98Handoff*,const Au98Hooks*,const MuBtOwner*,unsigned port);
int au98_tick(Au98Handoff*);
int au98_withdraw(Au98Handoff*,int (*barrier)(void*,const Inbound*),void*);
typedef struct {
 void *opaque;
 int (*original_image_verified)(void*);
 int (*on_original_iap2_owner)(void*);
 int (*original_iap2_service_verified)(void*);
 const Au100Evidence *route_evidence;
} Au98OwnerProof;
int au98_bind_stock_native(Au98Handoff*,const Au98OwnerProof*,const MuBtOwner*);
#endif
