/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU_INNER_SESSION_H
#define MU_INNER_SESSION_H
#include "inner_airplay.h"
#include "inner_socket.h"
/* Single owner-thread composition, no new protocol or OEM discovery.
 * Caller must independently verify the actual session, loaded library,
 * mm-ipod peer, and decoded dispatch with a response-status contract.
 * Zero initialize before first use. Successful start transfers fd ownership;
 * failure leaves it with caller. Library remains mapped through stop.
 * Keep this object across reconnects; each successful epoch must increase. */
typedef struct {
 MuInnerAirplay airplay;
 MuInnerSocket socket;
 void *ctx;
 int (*peer)(void *,int,uint64_t);
 uint64_t last_epoch;
 int active;
} MuInnerSession;
int mu_inner_session_start(MuInnerSession *,void *,void *,uint64_t,
 const MuInnerAirplayProof *,int,void *,int (*)(void *,int,uint64_t));
int mu_inner_session_tick(MuInnerSession *,uint64_t);
int mu_inner_session_control(MuInnerSession *,uint64_t,const void *,
 const void *,const void *);
void mu_inner_session_stop(MuInnerSession *);
/* For verified original _requestProcessCommand caller ONLY.
 * 0: unhandled, call stock SessionControl with all original arguments.
 * 1: handled, return *status to original caller. Busy/error nonzero.
 * On success leave the initially NULL response untouched: stock allocates
 * and serializes its normal empty dictionary. No hook or offset patch. */
int mu_inner_session_http_command(MuInnerSession *,uint64_t,void *,
 unsigned,const void *,const void *,const void *,void **,int *);
#endif
