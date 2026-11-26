#ifndef MU1005_NATIVE_EVENT_H
#define MU1005_NATIVE_EVENT_H
#include "inbound_r5.h"
#include <sys/types.h>
#include <stddef.h>
/* Explicit ARM32 LE event parsing. The adapter runs only in a verified OEM
 * callback context. No fixed address execution or arbitrary pointer reads.
 * owner callbacks must verify live SDK membership, lifetime and authorization.
 * All input pointers are borrowed for the duration of the native callback. */
typedef struct {
 void *opaque;
 int (*association_matches)(void *,const void *channel,uint32_t association,unsigned event_code);
 int (*read_live)(void *,uint32_t address,void *out,size_t length);
 int (*paired_authorized)(void *,uint32_t association,const uint8_t sdk_mac[6]);
} Mu38PeerOps;
/* vendor.opaque must point to this contract or an enclosing struct whose
 * first member is Mu38PeerOps; this function is passed to ir5_init. */
int mu38_resolve_peer(void *,const void *,const void *,uint8_t[6]);
/* Decode native callback; RX fetched into owned snapshot, event6 token passed
 * through (never dereferenced). Called only from certified native owner. */
int mu38_dispatch(InboundR5 *,const Mu38PeerOps *,void *channel,const void *event16);
/* Pinned MU1005 bt registry identity, equivalent membership/association
 * reads to 0x202610, 0x202640 and 0x276f74 without taking the vendor mutex.
 * Caller MUST independently hold the existing owner/lifetime/serialization
 * contract; stable snapshots do not establish lock exclusion or pairing.
 * No arbitrary process access, no native function call. */
int mu38_registry_association(uint32_t channel,uint32_t association,
 int (*read_live)(void *,uint32_t,void *,size_t),void *);
#endif
