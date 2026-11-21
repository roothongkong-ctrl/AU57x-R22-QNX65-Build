#ifndef MU1005_INBOUND_H
#define MU1005_INBOUND_H
/* Offline server-route candidate. No boot hook, no automatic activation. All
 * operations serialized on the certified native owner thread. ARM32 records
 * contain 32-bit addresses; host tests use explicit token mappings. */
#include <stdint.h>
#include <stddef.h>
enum { RF_SIZE=176, PACKET_SIZE=100, SDP_SIZE=36, MAX_DATA=5035 };
typedef struct { uint32_t words[RF_SIZE/4]; } RfObject;
typedef struct { uint32_t words[PACKET_SIZE/4]; } RfPacket;
typedef struct { uint32_t words[SDP_SIZE/4]; } SdpRecord;
typedef struct { uint16_t id,len; uint32_t value; uint16_t cache,reserved; } SdpAttr;
typedef struct {
 int (*reg)(void*,uint8_t*,uint16_t);
 int (*unreg)(void*,uint8_t*);
 int (*add)(void*); int (*remove)(void*);
 int (*accept)(void*,int); int (*limit)(void*); int (*send)(void*,void*);
 int (*disconnect)(void*);
 /* Must prove exact firmware, initialized SDK, exclusive object and owner thread. */
 int (*certify)(void*);
 /* Must verify actual paired/authorized peer; never auto-accept every device. */
 int (*peer_ok)(void*);
 /* Return 1 only if bounded transport queue is ready / accepted event. */
 int (*ready)(void*);
 int (*opened)(void*,unsigned);
 int (*closed)(void*);
 int (*rx)(void*,const uint8_t*,unsigned);
 int (*credit)(void*,int);
 uint32_t (*addr)(const void*); /* ARM32 native address or host test token */
 void *opaque;
 /* Set by R5 bridge: grant RX credit only on complete socket frame commit. */
 int defer_rx_credit;
} InboundOps;
typedef struct {
 RfObject rf; RfPacket packet; SdpRecord sdp,carplay;
 SdpAttr attrs[2], carattr;
 uint8_t svc[19], protocol[14], cpsvc[19], tx[MAX_DATA];
 uint8_t slot,state,pending,busy;
 uint8_t published_mask, uncertain_mask; /* vendor calls with known/unknown outcome */
 InboundOps ops;
} Inbound;
enum { IN_EMPTY,IN_PREPARED,IN_REGISTERED,IN_PUBLISHED,IN_LINK,IN_DIRTY,IN_WITHDRAWING,IN_RETIRED };
enum { IN_RF=1, IN_IAP_SDP=2, IN_CP_SDP=4 };
/* Zero-initialize stable instance ONCE; cannot move until OEM reclamation. */
int inbound_prepare(Inbound*,const InboundOps*,uint32_t callback_address);
int inbound_publish(Inbound*);
/* Owner-supervised only: barrier must cover callback exclusion, native idle,
 * no outstanding packets and service membership; no memory reclamation. */
int inbound_withdraw(Inbound*,int (*barrier)(void*,const Inbound*),void*);
int inbound_event(Inbound*,void* channel,unsigned code,unsigned status,
                  const uint8_t* payload,unsigned length,uint32_t packet_address);
int inbound_send(Inbound*,const uint8_t*,unsigned);
#endif
