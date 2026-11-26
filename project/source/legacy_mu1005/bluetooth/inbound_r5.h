#ifndef INBOUND_R5_H
#define INBOUND_R5_H
#include "inbound.h"
#include "mu1005_socket_pump.h"
/* Call only on an independently verified serialized SDK owner context.
 * The binder must deliver genuine events and resolve their peer from the OEM
 * connection/bond table; this module does not establish that trust itself.
 * No native address patch, no installer, one lifetime per connected socket. */
typedef int (*ResolvePeer)(void *,const void *,const void *,uint8_t [6]);
typedef struct {
 Inbound service; InboundOps vendor; AuRfIngress rf; AuIpcStream ipc;
 ResolvePeer resolve; int fd,initialized,dead,completion,authorized,had_link;
 uint8_t peer[6];
 uint32_t credits_committed;
 /* Actual native association identity accepted on event1, not a MAC alias.
  * Used only by native_event decoder; reset at genuine close. */
 uint32_t native_association;
 /* An accepted native link is tracked independently of the daemon stream. */
 int native_link, disconnect_attempted, disconnect_rc;
 /* R38 fixed owned storage: avoid 5 KB on the native callback stack. */
 uint8_t native_rx[MAX_DATA]; unsigned native_rx_busy;
} InboundR5;
int ir5_init(InboundR5 *,const InboundOps *,ResolvePeer,int,uint32_t);
int ir5_publish(InboundR5 *);
int ir5_discovery_ready(const InboundR5 *);
/* event is normalized by the separately verified native callback binder.
 * raw_event is used only by the binder's real peer resolution function. */
int ir5_event(InboundR5 *,void *,const void *,unsigned,unsigned,const uint8_t *,unsigned,uint32_t);
int ir5_tick(InboundR5 *);
/* Owner-only stop; 0 = known idle, -1 = wait, -2 = uncertain SDK state. */
int ir5_drain(InboundR5 *);
int ir5_withdraw(InboundR5 *,int (*barrier)(void*,const Inbound*),void*);
#endif
