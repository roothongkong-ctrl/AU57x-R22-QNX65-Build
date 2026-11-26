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
/* One caller-owned mailbox, stable storage until OEM callback drain.
 * Only the completion callback writes pending -> completed; owner loop
 * consumes it. No response object is retained. GCC atomic word operations
 * allow different HTTPClient and control queues. */
typedef struct { volatile int state; int status; } MuInnerEventFlight;
typedef struct {
 MuInnerAirplay airplay;
 MuInnerSocket socket;
 void *ctx;
 int (*peer)(void *,int,uint64_t);
 uint64_t last_epoch;
 MuInnerEventFlight *flight;
 uint64_t event_started_ms;
 int event_timer;
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
/* Original six-argument ABI. Only call from the verified HTTP command
 * callsite; never install this as a process-wide symbol interposer.
 * The original implementation must be a nonrecursive stock entry. */
typedef int (*MuInnerStockControl)(void *,unsigned,const void *,const void *,const void *,void **);
int mu_inner_session_dispatch_command(MuInnerSession *,uint64_t,MuInnerStockControl,
 void *,unsigned,const void *,const void *,const void *,void **);
/* Optional tracked sender, enabled before any packet is queued.
 * A completion is not an iAP2 ACK. At most one OEM request outstanding.
 * Mailbox must be zero-initialized and remain live after stop; reattach
 * only after independently confirmed old callback drain. */
int mu_inner_session_track_events(MuInnerSession *,MuInnerEventFlight *);
int mu_inner_event_reclaim(MuInnerEventFlight *,int original_drain_confirmed);
/* Monotonic owner tick for tracked sender. A provisional 5s completion
 * timeout retires the session; it does NOT cancel the original request. */
int mu_inner_session_tick_at(MuInnerSession *,uint64_t,uint64_t);
/* Only the exact ARM BL return address of the original HTTP command
 * handler may intercept; all other calls use the original target.
 * The load bias must come from an independently verified mapped image. */
int mu_inner_session_dispatch_at(MuInnerSession *,uint64_t,MuInnerStockControl,
 uintptr_t load_bias,uintptr_t caller_return,void *,unsigned,const void *,
 const void *,const void *,void **);
#endif
